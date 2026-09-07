// FleetComboRando.cpp — Generador del Combo Randomizer (Fases 2+3). Ver FleetComboRando.h.
//
// Flujo del thread de generación (GenerateCombo):
//   1. manifest del oráculo (bloqueante) -> checks MM, pool MM clasificado, starting items, opciones.
//   2. PrepareComboData: items FC ambos-lados (dedupe de ambos pools), triforce combo, split prog/junk MM.
//   3. Settings de SoH: SetAllToContext + forzar entrances OFF y songs Anywhere.
//   4. GenerateRandomizer nativo (síncrono en este thread). Dentro de Fill() corre nuestro
//      FleetCombo_PrePlacementHook por intento (assumed fill cross-game con el oráculo).
//   5. Spoiler MM (2S2H_RANDO_SPOILER) -> fleet_oracle_spoiler.json -> op prepareSeed.
//   6. SetSeedGenerated(true): el botón Randomizer de file select ya crea la seed combo en OoT;
//      el save pareado de MM aplica su spoiler vía OnFileCreate (mecanismo vanilla de 2ship).

#include "FleetComboRando.h"
#include "FleetShipCombo.h"
#include "FleetOracleClient.h"
#include "FleetComboItems.h"
#include "FleetComboItemsGlue.h"

#include "soh/Enhancements/randomizer/3drando/fill.hpp"
#include "soh/Enhancements/randomizer/3drando/item_pool.hpp"
#include "soh/Enhancements/randomizer/3drando/starting_inventory.hpp"
#include "soh/Enhancements/randomizer/3drando/menu.hpp"
#include "soh/Enhancements/randomizer/dungeon.h" // DungeonInfo (dungeon-item options)
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/randomizer/location_access.h"
#include "soh/Enhancements/randomizer/static_data.h"
#include "soh/Enhancements/randomizer/settings.h"
#include "soh/Enhancements/randomizer/item.h"
#include "soh/Enhancements/randomizer/logic.h"
#include "soh/Enhancements/custom-message/CustomMessageManager.h" // MF_CLEAN (area names)
#include <libultraship/libultraship.h> // Ship::Context::GetPathRelativeToAppDirectory (spoiler I/O)
#include <libultraship/bridge/consolevariablebridge.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <random>
#include <set>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace {

// ---------- estado ----------

std::atomic<bool> sRunning{ false };
std::atomic<bool> sComboActive{ false }; // gate del hook dentro de Fill()
std::thread sThread;
std::mutex sStatusMx;
std::string sStatus = "";

void SetStatus(const std::string& status) {
    std::lock_guard<std::mutex> lock(sStatusMx);
    sStatus = status;
    SPDLOG_INFO("[FleetComboRando] {}", status);
}

// datos del manifest de MM
struct MmCheck {
    int id;
    std::string name;
};
std::vector<MmCheck> sMmChecks;
std::unordered_map<int, std::string> sMmCheckName;
std::vector<std::string> sMmPoolProg; // progresión nativa MM (tras dedupe FC)
std::vector<std::string> sMmPoolJunk; // junk/health/trap de MM (relleno local)
// Copies of each RI name in MM's pool (pre-FC-dedupe). The set above loses multiplicity, and we need
// it to respect MM's Item Pool setting (Scarce/Balanced/Plentiful). Skijer's NEI
std::unordered_map<std::string, int> sMmManifestCounts;
std::unordered_set<std::string>
    sMmManifestPool; // TODOS los nombres RI del pool MM (pre-dedupe FC) —
                     // para el filtro per-juego: un FC MM-native solo cruza si su RI está barajado en MM
std::vector<std::string> sMmStarting;
nlohmann::json sMmOptions;                           // { "RO_X": value }
std::unordered_map<std::string, int> sMmCheckPrices; // 5.0.0: RC name -> shop price rolled by MM for this seed
std::vector<std::string> sMmSkippedChecks;           // 5.0.0: excluded checks MM turned into skipped junk

// items FC ambos-lados (cross-game)
struct ComboFcItem {
    int fcId;
    int rg;             // RandomizerGet (int) — lado OoT
    std::string riName; // "RI_*" — lado MM (spoilerName real de 2ship)
    int count;          // copias (nivel de cadena, v1)
    // PLACEMENT ELIGIBILITY PER SIDE. A world may only receive copies of an item it actually
    // SHUFFLES. When it does not, that world still places its own vanilla copy through its native
    // path, so dropping a cross copy there too supplies the item twice — that is what produced
    // "Song of Time: expected 1, got 2 (2 OoT + 0 MM)": OoT's song shuffle was off, so the songs were
    // absent from OoT's itemPool (the FC dedupe had nothing to erase) yet still crossed because MM
    // shuffles them. Skijer's NEI
    bool inOot = true;
    bool inMm = true;
};
std::vector<ComboFcItem> sComboFc;
bool sFcFilterApplied = false; // filtro per-juego aplicado a sComboFc (una vez por generación)

// Placement restriction for an FC item. The agreed rule: **the item's ORIGIN game decides** (OoT keys
// obey OoT's option, MM items obey MM's). Skijer's NEI
// An item with a placement restriction in OoT is NOT pre-placed: it stays in the native pool and the
// matching SoH stage places it. Trying to replicate "own dungeon" here was a mistake - binding a map
// to its dungeon's ~10 locations while placing in random order let the unrestricted items eat those
// slots first, so pre-placement aborted in a loop ("no candidates for RI_OOT_MAP_DEKU_TREE").
// RandomizeOwnDungeon already does that job, and in the correct order.
struct FcRestriction {
    bool noCross = false; // out of cross-placement; SoH's native stage places it
    int restrictCat = -1; // index into gFcCategories, or -1: bound to that category's spots in BOTH
                          // worlds (shared "Category Spots" mode)
    // MM-origin dungeon items (its small keys / boss keys / stray fairies) whose 2ship 5.0.0
    // placement option (RO_PLACEMENT_SMALL_KEYS / BOSS_KEYS / STRAY_FAIRIES) is NOT "Anywhere": every
    // copy is dealt to MM, whose own fillTurn confines it to its dungeon (IsItemAllowedAtCheck).
    // Letting them cross to Hyrule would silently defeat MM's own-dungeon setting.
    bool pinMm = false;
};

// ---- SHARED RESTRICTED CATEGORIES ----
//
// A category is a set of shared items that belongs to a matching set of SPOTS in both games — songs
// to song spots, dungeon rewards to boss spots. Each one has its own option with the same three
// modes, and the same machinery serves all of them:
//
//   0 OWN_GAME_LOGIC — no shared rule; OoT and MM each apply their own setting for that category.
//   1 CATEGORY_SPOTS — both games' spots become ONE pool and the items mix across worlds: an OoT
//                      song can land on an MM song spot, beating the Fire Temple can hand you
//                      Odolwa's Remains. Placed FIRST, in their own stage, and then IMMOVABLE:
//                      the items leave the shared pool and their spots leave the available set.
//   2 ANYWHERE       — ordinary shared items, wherever the fill puts them.
//
// Adding a category is one row here, one FCI_F_* flag on its items, and one predicate on the oracle
// side (FleetOracle.cpp, sOracleCategories) — nothing else. The `key` is what travels on the wire.
//
// SONGS: Double Time and Inverted Time stay out of the pool by design (FCI_F_SONG is not set on
// them). Goron Lullaby needs no forcing — MM already ships it as the 2-level progressive chain.
// REWARDS: 13 items (6 medallions + 3 stones + 4 remains) into 13 spots (OoT's 9 boss rewards +
// MM's 4). Skijer's NEI
enum FcRestrictMode { FC_RESTRICT_OWN_GAME_LOGIC = 0, FC_RESTRICT_CATEGORY_SPOTS = 1, FC_RESTRICT_ANYWHERE = 2 };

struct FcCategoryDef {
    const char* key;   // wire key, also the manifest's categoryChecks field
    const char* cvar;  // the shared option driving it
    const char* label; // for logs and the UI
    uint32_t itemFlag; // FCI_F_* marking the items that belong to it
};
const FcCategoryDef gFcCategories[] = {
    { "songs", "gFleetCombo.SharedSongs", "Songs", FCI_F_SONG },
    { "rewards", "gFleetCombo.SharedDungeonRewards", "Dungeon Rewards", FCI_F_DUNGEON_REWARD },
};
const int FC_CATEGORY_COUNT = (int)(sizeof(gFcCategories) / sizeof(gFcCategories[0]));

// Kept as the old names so the songs-only call sites elsewhere keep reading naturally.
enum FcSharedSongsMode {
    FC_SONGS_OWN_GAME_LOGIC = FC_RESTRICT_OWN_GAME_LOGIC,
    FC_SONGS_SONG_SPOTS = FC_RESTRICT_CATEGORY_SPOTS,
    FC_SONGS_ANYWHERE = FC_RESTRICT_ANYWHERE,
};

int FcCategoryMode(int cat) {
    int mode = CVarGetInteger(gFcCategories[cat].cvar, FC_RESTRICT_OWN_GAME_LOGIC);
    return (mode < 0 || mode > FC_RESTRICT_ANYWHERE) ? FC_RESTRICT_OWN_GAME_LOGIC : mode;
}

std::vector<FcRestriction> sFcRestrict; // parallel to sComboFc

// resultado de la pre-colocación (persiste al terminar Fill para el spoiler)
std::map<std::string, std::string> sMmPlacements; // RC_name -> RI_name

// Readable area of each MM check (RC_name -> "Woodfall Temple"), from the oracle manifest. Together
// with sMmPlacements it answers "where is this item?" when the item lives in MM. Skijer's NEI
std::map<std::string, std::string> sMmCheckAreas;

// MM's spots per restricted category (RC names), from the manifest. Its half of each shared pool.
std::vector<std::string> sMmCatChecks[FC_CATEGORY_COUNT];

// MM checks claimed by the restricted stages. They run before the delegated fill, so the turn loop
// has to start with these already marked as used or it would hand the same check out twice.
std::vector<std::string> sMmUsedByStage;

// WHERE the restricted stages put each item, per world. Spot AND item, because a pre-placed item is
// not a fact the moment it is placed - it becomes one when somebody REACHES its spot, exactly like a
// plando placement. Announcing them up front would be the same lie as handing them over as starting
// inventory: it lets a world assume something that may sit behind a check nobody can get to yet.
std::vector<std::pair<int, int>> sStagePlacedOot; // (RandomizerCheck, RandomizerGet)

// Same for MM's side: the CHECK as well as the item. The check is what the oracle needs in order to
// grant the item when its crawl reaches that spot, and what it reports back so the host can announce
// it to OoT at the right moment instead of up front. Skijer's NEI
std::vector<std::pair<std::string, int>> sStagePlacedMm; // (MM check name, RandomizerGet)

// Items a world had to place at home because they are NOT in the shared pool ("RI_x @ RC_y"). Dumped
// into the .fleet so a feature that lands upstream in either game shows up on the very first seed
// instead of going unnoticed for months, the way Progressive Strength did. Skijer's NEI
std::vector<std::string> sLocalOnlyMm;

std::mt19937 sRng;

uint64_t NowMs() {
    return (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

// ---------- oráculo bloqueante (corre en el thread de generación; el pump es por archivos+shm) ----------

nlohmann::json OracleBlocking(unsigned long long seq, const char* what, uint64_t timeoutMs = 60000) {
    if (seq == 0) {
        throw std::runtime_error(std::string("no active combo to request ") + what);
    }
    uint64_t start = NowMs();
    nlohmann::json resp;
    while (!FleetOracle_TryGetResponse(seq, resp)) {
        if (NowMs() - start > timeoutMs) {
            throw std::runtime_error(std::string("timeout waiting for ") + what + " from the oracle");
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    if (resp.contains("error")) {
        throw std::runtime_error("oracle: " + resp["error"].get<std::string>());
    }
    return resp;
}

// ---------- preparación de datos ----------

void ParseManifest(const nlohmann::json& manifest) {
    sMmChecks.clear();
    sMmCheckName.clear();
    sMmPoolProg.clear();
    sMmPoolJunk.clear();
    sMmManifestPool.clear();
    sMmManifestCounts.clear();
    sMmStarting.clear();
    sMmCheckAreas.clear();
    sMmOptions = nlohmann::json::object();

    // set de nombres RI cubiertos por FC ambos-lados (para dedupe del pool MM)
    std::unordered_set<std::string> fcPeerNames;
    for (int i = 0; i < FC_COMBO_ITEM_COUNT; i++) {
        if (FcCombo_NativeForItem(gFcComboItems[i].fcId) != FCI_NO_ITEM) {
            std::string peer = FcCombo_PeerNameForItem(gFcComboItems[i].fcId);
            if (peer != "FCI_NO_ITEM") {
                fcPeerNames.insert(peer);
            }
        }
    }

    for (auto& pair : manifest["checks"]) {
        sMmChecks.push_back({ pair[0].get<int>(), pair[1].get<std::string>() });
        sMmCheckName[pair[0].get<int>()] = pair[1].get<std::string>();
    }
    // Readable area per MM check (RC_name -> "Woodfall Temple"). Hint generation uses it to describe
    // items placed in MM instead of falling back to "Invalid Location". Optional: 2ship builds older
    // than this change do not send it. Skijer's NEI
    // MM's spots for each restricted category, keyed by the same `key` the category table uses. They
    // are MM's contribution to each shared spot pool, which is what decides how many items of that
    // category each world receives. Skijer's NEI
    for (int cat = 0; cat < FC_CATEGORY_COUNT; cat++) {
        sMmCatChecks[cat].clear();
    }
    if (manifest.contains("categoryChecks") && manifest["categoryChecks"].is_object()) {
        for (int cat = 0; cat < FC_CATEGORY_COUNT; cat++) {
            auto it = manifest["categoryChecks"].find(gFcCategories[cat].key);
            if (it == manifest["categoryChecks"].end() || !it->is_array()) {
                continue;
            }
            for (auto& n : *it) {
                sMmCatChecks[cat].push_back(n.get<std::string>());
            }
            SPDLOG_INFO("[FleetComboRando] manifest: {} MM spots for category '{}'", sMmCatChecks[cat].size(),
                        gFcCategories[cat].key);
        }
    } else if (manifest.contains("songChecks") && manifest["songChecks"].is_array()) {
        // 2ship builds older than categoryChecks only send the song list. Songs still work; any other
        // category simply has no MM spots and stays inside OoT. Skijer's NEI
        for (auto& n : manifest["songChecks"]) {
            sMmCatChecks[0].push_back(n.get<std::string>());
        }
        SPDLOG_WARN("[FleetComboRando] manifest has no categoryChecks (old 2ship build): {} song spots only",
                    sMmCatChecks[0].size());
    }
    if (manifest.contains("checkAreas") && manifest["checkAreas"].is_object()) {
        for (auto& [rcName, area] : manifest["checkAreas"].items()) {
            if (area.is_string()) {
                sMmCheckAreas[rcName] = area.get<std::string>();
            }
        }
        SPDLOG_INFO("[FleetComboRando] manifest: {} MM check areas for hints", sMmCheckAreas.size());
    } else {
        SPDLOG_WARN("[FleetComboRando] manifest without checkAreas - hints for items in MM will be generic");
    }
    // 5.0.0: shop/Tingle prices MM rolled for this seed (spoiler must carry them as {randoItemId, price}
    // or every shop item applies at 0 rupees), and the excluded checks MM turned into skipped junk
    // (spoiler must mark them, or the apply hands out their vanilla item a second time).
    sMmCheckPrices.clear();
    sMmSkippedChecks.clear();
    if (manifest.contains("checkPrices") && manifest["checkPrices"].is_object()) {
        for (auto& [rcName, price] : manifest["checkPrices"].items()) {
            if (price.is_number()) {
                sMmCheckPrices[rcName] = price.get<int>();
            }
        }
    }
    if (manifest.contains("skippedChecks") && manifest["skippedChecks"].is_array()) {
        for (auto& n : manifest["skippedChecks"]) {
            if (n.is_string()) {
                sMmSkippedChecks.push_back(n.get<std::string>());
            }
        }
    }
    SPDLOG_INFO("[FleetComboRando] manifest: {} MM shop prices, {} skipped (excluded) checks", sMmCheckPrices.size(),
                sMmSkippedChecks.size());
    for (auto& entry : manifest["pool"]) {
        std::string name = entry[0].get<std::string>();
        std::string cat = entry[1].get<std::string>();
        sMmManifestPool.insert(name); // TODO nombre del pool MM (incl. los que FC dedupe) para el filtro per-juego
        sMmManifestCounts[name]++;    // multiplicity: how many copies MM's Item Pool asks for
        if (name == "RI_TRIFORCE_PIECE" || fcPeerNames.contains(name)) {
            continue; // FC lo aporta (1 copia global) o lo controla el goal combo
        }
        if (cat == "prog") {
            sMmPoolProg.push_back(name);
        } else {
            sMmPoolJunk.push_back(name);
        }
    }
    for (auto& name : manifest["startingItems"]) {
        sMmStarting.push_back(name.get<std::string>());
    }
    for (auto& opt : manifest["options"]) { // [name, cvar, value]
        sMmOptions[opt[0].get<std::string>()] = opt[2].get<int64_t>();
    }
}

void PrepareComboFcItems() {
    sComboFc.clear();
    sFcRestrict.clear();      // recomputed from this generation's options
    sFcFilterApplied = false; // se recomputa el filtro per-juego en la 1ª pasada de pre-colocación
    for (int i = 0; i < FC_COMBO_ITEM_COUNT; i++) {
        const FcComboItemInfo& info = gFcComboItems[i];
        // corazones/traps/triforce se manejan aparte (economías nativas / goal combo)
        if ((info.flags & FCI_F_TRAP) || (info.flags & FCI_F_TRIFORCE) ||
            std::string(info.comboName).rfind("Piece of Heart", 0) == 0 ||
            std::string(info.comboName).rfind("Heart Container", 0) == 0) {
            continue;
        }
        int rg = FcCombo_NativeForItem(info.fcId);
        std::string riName = FcCombo_PeerNameForItem(info.fcId);
        if (rg == FCI_NO_ITEM || riName == "FCI_NO_ITEM") {
            continue; // solo los AMBOS-lados son cross; RG-only quedan en el pool nativo de SoH,
                      // RI-only quedan en el pool nativo de MM (manifest)
        }
        sComboFc.push_back({ info.fcId, rg, riName, (int)info.chainLen });
    }
    // Goal Triforce Hunt: piezas al pool cross (ambos-lados por la tabla FC)
    if (CVarGetInteger("gFleetCombo.GoalMode", 0) == 1) {
        int total = CVarGetInteger("gFleetCombo.TriforceTotal", 15);
        int rg = FcCombo_NativeForItem(FCI_TRIFORCE_PIECE);
        std::string riName = FcCombo_PeerNameForItem(FCI_TRIFORCE_PIECE);
        if (rg != FCI_NO_ITEM && riName != "FCI_NO_ITEM" && total > 0) {
            sComboFc.push_back({ FCI_TRIFORCE_PIECE, rg, riName, total });
        }
    }
}

// Translates OoT's dungeon-item options into per-FC-item restrictions.
//
// Pre-placement used to check only "location not banned and empty", so keys ended up in Hyrule Field
// under Own Dungeon and medallions in Termina grass under End of Dungeons.
//
// Only OoT-ORIGIN items are restricted (riName prefixed "RI_OOT_": their MM side exists only because
// the combo imported it). Genuinely shared items (bow, hookshot) and MM-origin ones stay free - their
// own game sets their rules. Skijer's NEI
void BuildFcRestrictions() {
    auto ctx = Rando::Context::GetInstance();
    sFcRestrict.assign(sComboFc.size(), FcRestriction{});

    // Shared restricted categories. Resolved BEFORE the delegated restrictions below so a category
    // wins inside its own domain: in Category Spots mode its items are bound to its spots no matter
    // what either game's own setting for them says. In the other two modes nothing is marked and the
    // items behave like any other shared item.
    //
    // A row can only belong to ONE category (the first that claims it); the flags are disjoint by
    // construction, and marking twice would make the stage below place it twice.
    for (int cat = 0; cat < FC_CATEGORY_COUNT; cat++) {
        const int mode = FcCategoryMode(cat);
        if (mode != FC_RESTRICT_CATEGORY_SPOTS) {
            SPDLOG_INFO("[FleetComboRando] shared category '{}': {}", gFcCategories[cat].label,
                        mode == FC_RESTRICT_ANYWHERE ? "Anywhere, items cross as ordinary items"
                                                     : "Own Game Logic, each game applies its own setting");
            continue;
        }
        size_t marked = 0;
        for (size_t i = 0; i < sComboFc.size(); i++) {
            if (sFcRestrict[i].restrictCat >= 0) {
                continue;
            }
            for (int k = 0; k < FC_COMBO_ITEM_COUNT; k++) {
                if (gFcComboItems[k].fcId == sComboFc[i].fcId) {
                    if (gFcComboItems[k].flags & gFcCategories[cat].itemFlag) {
                        sFcRestrict[i].restrictCat = cat;
                        marked++;
                    }
                    break;
                }
            }
        }
        SPDLOG_INFO("[FleetComboRando] shared category '{}': Category Spots, {} items bound to its spots",
                    gFcCategories[cat].label, marked);
    }

    // MM's OWN dungeon-item placement options (2ship 5.0.0). The manifest carries MM's option values
    // (sMmOptions); anything but "Anywhere" (RO_DUNGEON_ITEM_ANYWHERE == 0) pins that item family to
    // MM, where 2ship's fillTurn enforces the confinement itself. Values mirror mm/2s2h/Rando/Types.h:
    // RO_DUNGEON_ITEM_ANYWHERE=0, OWN_DUNGEON=1, START_WITH=2 (only "0 = free" is relied on here).
    {
        auto mmOpt = [&](const char* name) -> int {
            return sMmOptions.is_object() && sMmOptions.contains(name) && sMmOptions[name].is_number()
                       ? sMmOptions[name].get<int>()
                       : 0;
        };
        const bool pinSmall = mmOpt("RO_PLACEMENT_SMALL_KEYS") != 0;
        const bool pinBoss = mmOpt("RO_PLACEMENT_BOSS_KEYS") != 0;
        const bool pinFairy = mmOpt("RO_PLACEMENT_STRAY_FAIRIES") != 0;
        int pinned = 0;
        for (size_t i = 0; i < sComboFc.size(); i++) {
            const std::string& ri = sComboFc[i].riName;
            if (ri.rfind("RI_OOT_", 0) == 0) {
                continue; // OoT-origin: handled by OoT's own options below
            }
            bool isSmall = ri.size() > 10 && ri.compare(ri.size() - 10, 10, "_SMALL_KEY") == 0;
            bool isBoss = ri.size() > 9 && ri.compare(ri.size() - 9, 9, "_BOSS_KEY") == 0;
            bool isFairy = ri.size() > 12 && ri.compare(ri.size() - 12, 12, "_STRAY_FAIRY") == 0 &&
                           ri != "RI_CLOCK_TOWN_STRAY_FAIRY"; // not a dungeon item
            if ((isSmall && pinSmall) || (isBoss && pinBoss) || (isFairy && pinFairy)) {
                sFcRestrict[i].pinMm = true;
                pinned++;
            }
        }
        if (pinned > 0) {
            SPDLOG_INFO(
                "[FleetComboRando] MM dungeon-item placement: {} FC rows pinned to MM (small={} boss={} fairies={})",
                pinned, pinSmall, pinBoss, pinFairy);
        }
    }

    // Fast index rg -> positions in sComboFc
    std::unordered_map<int, std::vector<size_t>> byRg;
    for (size_t i = 0; i < sComboFc.size(); i++) {
        if (sComboFc[i].riName.rfind("RI_OOT_", 0) == 0) { // OoT-origin items only
            byRg[sComboFc[i].rg].push_back(i);
        }
    }
    if (byRg.empty()) {
        return;
    }

    auto markNoCross = [&](RandomizerGet rg) {
        if (rg == RG_NONE) {
            return;
        }
        for (size_t i : byRg[(int)rg]) {
            // A shared category OUTRANKS the game's own setting inside its domain — that is the whole
            // point of the option. Marking noCross on top would be worse than redundant: noCross rows
            // are skipped by the dedupe and stay in OoT's native pool, so the restricted stage would
            // place its copy AND the native fill another. Exactly the Link's Pocket duplicate again.
            if (sFcRestrict[i].restrictCat >= 0) {
                continue;
            }
            sFcRestrict[i].noCross = true;
        }
    };
    const uint8_t keysanity = ctx->GetOption(RSK_KEYSANITY).Get();
    const uint8_t bossKeys = ctx->GetOption(RSK_BOSS_KEYSANITY).Get();
    const uint8_t ganonKey = ctx->GetOption(RSK_GANONS_BOSS_KEY).Get();
    const uint8_t mapCompass = ctx->GetOption(RSK_SHUFFLE_MAPANDCOMPASS).Get();
    const uint8_t rewards = ctx->GetOption(RSK_SHUFFLE_DUNGEON_REWARDS).Get();

    // ONLY "Anywhere" allows cross-placement. Every other option (own dungeon, any dungeon,
    // overworld, vanilla, start-with) binds the item to a subset of OoT locations, and SoH's native
    // stages already handle that - and they place those items BEFORE the rest, so unrestricted items
    // cannot steal their slots.
    auto applyDungeonOption = [&](uint8_t option, RandomizerGet rg) {
        if (option != RO_DUNGEON_ITEM_LOC_ANYWHERE) {
            markNoCross(rg);
        }
    };

    for (auto dungeon : ctx->GetDungeons()->GetDungeonList()) {
        applyDungeonOption(keysanity, dungeon->GetSmallKey());
        applyDungeonOption(keysanity, dungeon->GetKeyRing());
        applyDungeonOption(mapCompass, dungeon->GetMap());
        applyDungeonOption(mapCompass, dungeon->GetCompass());
        if (dungeon->GetBossKey() == RG_GANONS_CASTLE_BOSS_KEY) {
            applyDungeonOption(ganonKey, dungeon->GetBossKey());
        } else {
            applyDungeonOption(bossKeys, dungeon->GetBossKey());
        }
        // Rewards: same rule, only Anywhere crosses.
        if (rewards != RO_DUNGEON_REWARDS_ANYWHERE) {
            markNoCross(dungeon->GetReward());
        }
    }

    int noCross = 0;
    for (auto& r : sFcRestrict) {
        noCross += r.noCross ? 1 : 0;
    }
    SPDLOG_INFO("[FleetComboRando] FC restrictions: {} items out of cross-placement (native SoH places them)", noCross);
}

// ---------- delegated fill (runs INSIDE Fill(), once per attempt) ----------
//
// The combo no longer places anything on MM's behalf. It only decides WHICH GAME each shared item
// belongs to, and then both games fill themselves with their own native logic, turn by turn:
//   OoT's turn -> its own ReachabilitySearch picks slots among OoT locations
//   MM's turn  -> the fillTurn oracle op, which uses 2ship's own shuffled checkPool
// Each side treats what the other already placed as a kept promise. Everything is seeded, so the
// same seed always yields the same game. Skijer's NEI

// Applies the per-game FC filter exactly once per generation. Split out of RunDelegatedFill because
// the restricted stages below need it too, and they now run EARLIER (see RunRestrictedStages).
void ApplyFcPerGameFilter() {
    auto ctx = Rando::Context::GetInstance();

    // FILTRO PER-JUEGO (respeta las opciones de randomizer de cada juego): un item FC solo se
    // cross-coloca si está BARAJADO en su juego de origen — su RG en el itemPool de OoT, o su RI en
    // el pool del manifest de MM. Si no está en ninguno (categoría en vanilla/off) se OMITE: se queda
    // en su colocación nativa, sin duplicarse. El goal Triforce combo siempre se conserva.
    // Se aplica una sola vez por generación (aquí el itemPool aún está completo, antes del dedupe).
    if (!sFcFilterApplied) {
        sFcFilterApplied = true;
        std::unordered_set<int> ootPoolRgs;
        for (RandomizerGet rg : itemPool) {
            ootPoolRgs.insert((int)rg);
        }
        // Real multiplicity of OoT's pool (already reflects Scarce/Balanced/Plentiful, because
        // AddItemToPool inserts `count` copies according to RSK_ITEM_POOL).
        std::unordered_map<int, int> ootPoolCounts;
        for (RandomizerGet rg : itemPool) {
            ootPoolCounts[(int)rg]++;
        }

        // RGs that OoT has a VANILLA SOURCE for — an item sitting at one of its own locations. This is
        // what decides eligibility, NOT pool membership: the risk being avoided is supplying a copy of
        // something the world already hands out by itself. An MM-native item cross-imported into OoT
        // (Sonata, Powder Keg, the clock halves...) has no vanilla source there, so OoT can always
        // receive it. Testing pool membership instead read those as "not shuffled" and pinned every
        // MM item to Termina — a combo where nothing from MM ever appears in Hyrule. Skijer's NEI
        std::unordered_set<int> ootVanillaSourced;
        for (RandomizerCheck rc : ctx->allLocations) {
            auto* loc = Rando::StaticData::GetLocation(rc);
            if (loc != nullptr && loc->GetVanillaItem() != RG_NONE) {
                ootVanillaSourced.insert((int)loc->GetVanillaItem());
            }
        }

        std::vector<ComboFcItem> active;
        for (auto& it : sComboFc) {
            // Rows marked FCI_F_NOT_SHARED are deliberately local. They must be dropped BEFORE the
            // dedupe, or step 1 would erase every copy of the RG from OoT's pool and the row would put
            // back a single one — turning 24 scattered bombchus into 1. Skijer's NEI
            const FcComboItemInfo* flagInfo = nullptr;
            for (int k = 0; k < FC_COMBO_ITEM_COUNT; k++) {
                if (gFcComboItems[k].fcId == it.fcId) {
                    flagInfo = &gFcComboItems[k];
                    break;
                }
            }
            if (flagInfo != nullptr && (flagInfo->flags & FCI_F_NOT_SHARED)) {
                continue;
            }
            // MAGIC BEAN: OoT ships it in one of TWO MUTUALLY EXCLUSIVE shapes, chosen by
            // RSK_SHUFFLE_MERCHANTS — the 10-bean Pack goes in the pool, or a single Magic Bean is
            // placed on the salesman (item_pool.cpp). MM has one item for both (RI_MAGIC_BEAN), and a
            // second FC row is impossible: two rows sharing a peer name collapse in riToFc and the
            // survivor double-supplies. So the single row points at whichever shape this seed uses.
            // With the vanilla-salesman shape the eligibility rule below then keeps OoT's own bean
            // where it is and sends the shared copy to Termina. Skijer's NEI
            if (it.fcId == FCI_MAGIC_BEAN_PACK && !ootPoolRgs.contains((int)RG_MAGIC_BEAN_PACK)) {
                it.rg = (int)RG_MAGIC_BEAN;
            }
            bool inOot = ootPoolRgs.contains(it.rg);
            bool inMm = sMmManifestPool.contains(it.riName);
            // An item crosses if EITHER game shuffles it.
            //
            // There was briefly a stricter rule here (OoT-origin items required inOot) meant to stop
            // MM's permissive pool from overriding OoT's own-dungeon restriction. It was too broad and
            // silently DROPPED every OoT-origin item that simply is not in OoT's shuffled pool —
            // Progressive Master Sword, Hammer and Roc's Feather vanished from cross-placement, and
            // the Ganondorf hint pointing at the Master Sword degraded to "an Isolated Place".
            //
            // Placement restrictions are already handled properly by `noCross` (BuildFcRestrictions),
            // which keeps restricted items out of cross-placement entirely and lets SoH's native
            // stages place them. This gate was redundant on top of that. Skijer's NEI
            if (!inOot && !inMm && it.fcId != FCI_TRIFORCE_PIECE) {
                // Dropped: neither game shuffles it, so there is nothing to cross-place. Log it —
                // this is exactly how Progressive Master Sword and Progressive Strength went missing
                // from every seed, and without naming them here it is invisible. Skijer's NEI
                SPDLOG_WARN("[FleetComboRando] shared item NOT crossing: {} (rg={} not in OoT pool, {} not in MM pool)",
                            it.riName, it.rg, it.riName);
                continue;
            }

            // COPY COUNT. This used to be a fixed `chainLen`, which happens to be exactly SoH's
            // PLENTIFUL count (bow 4, magic 3...). Effect: Balanced behaved like Plentiful, and Scarce
            // injected two or three times the copies the option asks for - with ~180 cross-game items
            // that floods the fill with progression, which is why Scarce seeds took forever or never
            // converged.
            //
            // Now it comes from each game's REAL pool (which already applied its Item Pool setting).
            // We take the MAXIMUM of the two: the pool is shared (1 copy serves both games), so we
            // must supply enough for the hungrier game to reach its top level; with the minimum that
            // game could never climb the chain. Never above chainLen: more copies than levels adds
            // nothing. Skijer's NEI
            if (it.fcId != FCI_TRIFORCE_PIECE) {
                int ootCount = ootPoolCounts.contains(it.rg) ? ootPoolCounts[it.rg] : 0;
                auto mmIt = sMmManifestCounts.find(it.riName);
                int mmCount = mmIt != sMmManifestCounts.end() ? mmIt->second : 0;
                // A chain's copies are its UPGRADE LEVELS, not duplicates: Progressive Biggoron's
                // Sword is one chain where copy 1 gives the Biggoron Sword and copy 2 upgrades it to
                // the Great Fairy Sword. Cutting below chainLen therefore deletes the top of the
                // chain outright and makes that item unobtainable, which is never what an Item Pool
                // setting should do. So chained items (chainLen > 1) always get their full length;
                // only single-copy items follow the pools. Skijer's NEI
                // TWO KINDS OF SHARED ITEM, and they count differently:
                //
                //  (a) BOTH games hold the same thing (ootName == mmName, e.g. "Progressive Bow").
                //      One copy serves both worlds, so the count follows the pools and the Item Pool
                //      setting keeps its bite.
                //
                //  (b) Each game contributes a DIFFERENT LINK of one chain ("Progressive Biggoron's
                //      Sword" on OoT's side, "Great Fairy Sword" on MM's). Copy 1 gives the Biggoron
                //      Sword and copy 2 upgrades it, so the chain must keep its full length: cutting
                //      it deletes the top link and makes that item unobtainable. Same for Progressive
                //      Master Sword.
                //
                // Telling them apart from the names is exact, and it means no chain has to be
                // sacrificed to keep Scarce meaningful. Skijer's NEI
                const FcComboItemInfo* info = nullptr;
                for (int k = 0; k < FC_COMBO_ITEM_COUNT; k++) {
                    if (gFcComboItems[k].fcId == it.fcId) {
                        info = &gFcComboItems[k];
                        break;
                    }
                }
                bool sameItemBothSides = info != nullptr && info->ootName != nullptr && info->mmName != nullptr &&
                                         info->ootName[0] != '\0' &&
                                         std::string(info->ootName) == std::string(info->mmName);

                int wanted = std::max(ootCount, mmCount);
                if (wanted > 0 && sameItemBothSides) {
                    it.count = std::min(wanted, it.count); // (a) pools decide, capped at chainLen
                } else {
                    // (b) chainLen is a FLOOR here, not a cap. The table's idea of how long a chain is
                    // can be shorter than what the game actually needs, because the length depends on
                    // options: with Grab shuffled, the first Progressive Strength grants Grab and only
                    // the fourth reaches Golden Gauntlets — the row says 3, OoT's pool says 4.
                    // Supplying 3 left HasStrength(3) permanently false, which sealed Ganon's Tower and
                    // failed every attempt at exactly 2488 of 2516 locations, with nothing pointing at
                    // strength. Never hand a world fewer copies than its own pool holds. Skijer's NEI
                    it.count = std::max(it.count, wanted);
                }
                // (b) leaves it.count at chainLen: every link of the chain exists.
                // wanted == 0 cannot happen here (inOot || inMm guarantees at least one copy), but if
                // it did we keep chainLen rather than placing zero copies of a progression item and
                // breaking the logic.
            }
            // ELIGIBILITY (which world may RECEIVE copies) is not the same question as membership of
            // the shared pool. A world is eligible unless it would hand the item out by itself:
            // it is in that world's shuffled pool (fine, the pool copy IS the shuffled one), or the
            // world has no vanilla source at all (nothing to duplicate). Only "has a vanilla source
            // but is not shuffled" is ineligible — that is the case where the world keeps its copy
            // where it has always been and a cross copy would be a second one.
            // The Triforce goal is combo-owned: neither game places it natively, always eligible.
            it.inOot = inOot || !ootVanillaSourced.contains(it.rg) || it.fcId == FCI_TRIFORCE_PIECE;
            // MM is always eligible. Its manifest pool is built FROM its checks' vanilla items, so
            // "in the pool" and "has a vanilla source" are the same set on that side — there is no
            // has-a-source-but-not-shuffled case to protect against, and treating pool membership as
            // eligibility would pin every OoT item to Hyrule (Zelda's Lullaby, the BGS chain and the
            // Pendant all came back mm=0). The one exception the manifest cannot express is MM shops
            // with shuffle off; if a duplicate ever shows up there, it needs a real flag, not a guess.
            // (`inMm` above stays in use for the does-anyone-shuffle-this drop check and the counts.)
            it.inMm = true;
            active.push_back(it);
        }
        sComboFc = std::move(active);
        BuildFcRestrictions();
    }
}

// RESTRICTED CATEGORY STAGES — the FIRST thing that places anything, before every native stage.
//
// They used to run from the pre-placement hook, which is late: own-dungeon items, dungeon rewards,
// Link's Pocket and the excluded-locations junk fill had all already run, and any of them could sit
// on a song spot or a boss reward. The stage then found fewer free spots than items and blamed
// reachability. Reserving the spots stage by stage was whack-a-mole — one reservation existed for
// songs, none for rewards, and nothing covers a stage added later.
//
// Running FIRST removes the whole class of problem: once these spots hold an item, every native stage
// skips them on its own, because they only ever fill EMPTY locations. No reservation needed anywhere.
//
// Returns false to retry the attempt (never relaxes a restriction). Skijer's NEI
std::filesystem::path FleetDir(); // defined further down; the plando reader below needs it early

// Checked ONCE, before generation starts, and it throws so the message reaches the user.
//
// The fill's own hooks cannot report this: a false return there means "retry", so a typo in the file
// would burn all 30 attempts and end with a generic "could not find a valid placement" — the one
// thing the author needs to know (which name is wrong) nowhere in sight. Validating up front means
// the run stops immediately and says exactly what it could not find, all of it at once rather than
// one name per attempt. Skijer's NEI
void ValidatePartialPlando() {
    std::filesystem::path path = FleetDir() / "plando.json";
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        return;
    }
    nlohmann::json plando;
    try {
        std::ifstream in(path);
        in >> plando;
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("plando.json is not valid JSON: ") + e.what());
    }

    std::unordered_set<std::string> locNames, itemNames;
    for (int rc = RC_UNKNOWN_CHECK + 1; rc < RC_MAX; rc++) {
        auto* loc = Rando::StaticData::GetLocation((RandomizerCheck)rc);
        if (loc != nullptr && !loc->GetName().empty()) {
            locNames.insert(loc->GetName());
        }
    }
    for (int rg = RG_NONE + 1; rg < RG_MAX; rg++) {
        itemNames.insert(Rando::StaticData::RetrieveItem((RandomizerGet)rg).GetName().GetEnglish());
    }
    std::set<std::string> mmChecks;
    for (auto& [id, name] : sMmCheckName) {
        mmChecks.insert(name);
    }

    std::string problems;
    auto note = [&](const std::string& what) {
        problems += problems.empty() ? "" : "; ";
        problems += what;
    };
    if (plando.contains("oot") && plando["oot"].is_object()) {
        for (auto& [checkName, itemJson] : plando["oot"].items()) {
            if (!itemJson.is_string()) {
                note("OoT check '" + checkName + "' does not name an item");
                continue;
            }
            if (!locNames.contains(checkName)) {
                note("no OoT location called '" + checkName + "'");
            }
            if (!itemNames.contains(itemJson.get<std::string>())) {
                note("no OoT item called '" + itemJson.get<std::string>() + "'");
            }
        }
    }
    if (plando.contains("mm") && plando["mm"].is_object()) {
        for (auto& [checkName, itemJson] : plando["mm"].items()) {
            if (!itemJson.is_string()) {
                note("MM check '" + checkName + "' does not name an item");
                continue;
            }
            // MM checks come from the manifest, so this also catches a check its options excluded
            // from this seed — which is just as unplaceable as a typo.
            if (!mmChecks.contains(checkName)) {
                note("MM has no check '" + checkName + "' in this seed");
            }
        }
    }
    if (!problems.empty()) {
        throw std::runtime_error("plando.json: " + problems);
    }
}

// PARTIAL PLANDO — fix some checks by hand, randomise everything else.
//
// SoH's own Plandomizer is not this: it exports a spoiler with EVERY location filled and you load it
// with ParseSpoiler, no generation involved. The combo already covers that case — a .fleet holds both
// worlds' spoilers and FleetCombo_LoadFleet bakes them in. What neither game has is fixing a handful
// of items and letting the fill do the rest.
//
// It costs almost nothing here because it is the same shape as a restricted-category placement: an
// item that is already in the world before the race starts. So it reuses that machinery wholesale —
// out of the shared pool via the dedupe, skipped by every native stage (they only fill empty
// locations), sent to MM as `prePlaced` so its crawl collects it on arrival, and announced to the
// other world only when somebody actually reaches it.
//
// <ShipDir>/fleet/plando.json:  { "oot": { "<Location Name>": "<Item Name>" },
//                                 "mm":  { "RC_CHECK_NAME": "RI_ITEM_NAME" } }
// Absent file = nothing fixed. Skijer's NEI
bool ApplyPartialPlando() {
    std::filesystem::path path = FleetDir() / "plando.json";
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        return true;
    }
    nlohmann::json plando;
    try {
        std::ifstream in(path);
        in >> plando;
    } catch (const std::exception& e) {
        // Throw rather than return false: a false here means "retry the attempt", and no number of
        // retries fixes a malformed file.
        throw std::runtime_error(std::string("plando.json could not be read: ") + e.what());
    }

    auto ctx = Rando::Context::GetInstance();
    size_t oot = 0, mm = 0;

    if (plando.contains("oot") && plando["oot"].is_object()) {
        // Reverse lookups by display name, built once. The plando file is written by a human, so it
        // names things the way the spoiler does rather than by enum id.
        std::unordered_map<std::string, int> locByName, itemByName;
        for (RandomizerCheck rc : ctx->allLocations) {
            auto* loc = Rando::StaticData::GetLocation(rc);
            if (loc != nullptr) {
                locByName[loc->GetName()] = (int)rc;
            }
        }
        for (int rg = RG_NONE + 1; rg < RG_MAX; rg++) {
            itemByName[Rando::StaticData::RetrieveItem((RandomizerGet)rg).GetName().GetEnglish()] = rg;
        }
        for (auto& [checkName, itemJson] : plando["oot"].items()) {
            auto locIt = locByName.find(checkName);
            auto itemIt = itemByName.find(itemJson.get<std::string>());
            if (locIt == locByName.end() || itemIt == itemByName.end()) {
                // ValidatePartialPlando already vetted every name against the static tables, so the
                // only way to land here is a location the STATIC table has but this seed's pool does
                // not — an option excluded it. Worth saying, and worth stopping for: silently
                // dropping a fixed placement is the one thing a plando author must never get.
                throw std::runtime_error("plando.json: '" + checkName + "' is not a location this seed shuffles");
            }
            ctx->PlaceItemInLocation((RandomizerCheck)locIt->second, (RandomizerGet)itemIt->second);
            sStagePlacedOot.push_back({ locIt->second, itemIt->second });
            oot++;
        }
    }

    if (plando.contains("mm") && plando["mm"].is_object()) {
        std::set<std::string> knownChecks;
        for (auto& [id, name] : sMmCheckName) {
            knownChecks.insert(name);
        }
        for (auto& [checkName, itemJson] : plando["mm"].items()) {
            if (!knownChecks.contains(checkName)) {
                throw std::runtime_error("plando.json: MM has no check '" + checkName + "'");
            }
            std::string riName = itemJson.get<std::string>();
            sMmPlacements[checkName] = riName;
            sMmUsedByStage.push_back(checkName);
            // Only FC rows can be announced to OoT; an MM-local item still occupies the check but
            // means nothing to Hyrule, which is correct.
            for (auto& row : sComboFc) {
                if (row.riName == riName) {
                    sStagePlacedMm.push_back({ checkName, row.rg });
                    break;
                }
            }
            mm++;
        }
    }

    if (oot + mm > 0) {
        SPDLOG_INFO("[FleetComboRando] partial plando: {} fixed in OoT, {} fixed in MM", oot, mm);
    }
    return true;
}

bool RunRestrictedStages() {
    auto ctx = Rando::Context::GetInstance();
    // This is now the first combo code to run in an attempt, so it owns resetting the per-attempt
    // state that used to be cleared at the top of RunDelegatedFill. Unconditionally: a leftover
    // sMmPlacements from a failed attempt would be read as real placements by the next one.
    sMmPlacements.clear();
    sMmUsedByStage.clear();
    sStagePlacedMm.clear();
    sStagePlacedOot.clear();

    // Hand-fixed placements go down BEFORE the categories, so a category can never claim a spot the
    // author reserved — the stages only ever pick spots that are still empty.
    if (!ApplyPartialPlando()) {
        return false;
    }

    // NO per-game filter here. It judges "is this item shuffled in its own game?" by looking at
    // OoT's itemPool, and that answer is only right AFTER the native restricted stages have taken
    // their own items out of it — that is how own-dungeon keys, maps, compasses and key rings stay
    // out of the shared pool. Running it this early made 69 rows cross that never crossed before and
    // broke every seed on playthroughBeatable. It stays where it was, in RunDelegatedFill.
    //
    // These stages do not need it: they work off the FULL table PrepareComboFcItems built, which is a
    // superset — and assuming a superset is exactly what an assumed fill wants. Skijer's NEI
    bool anyRestricted = false;
    for (int cat = 0; cat < FC_CATEGORY_COUNT; cat++) {
        anyRestricted = anyRestricted || FcCategoryMode(cat) == FC_RESTRICT_CATEGORY_SPOTS;
    }
    if (!anyRestricted) {
        return true;
    }

    // Portal sentinels, same as the turn loop uses.
    std::vector<RandomizerCheck> searchLocations = ctx->allLocations;
    searchLocations.push_back(RC_ALTAR_HINT_CHILD);
    searchLocations.push_back(RC_ALTAR_HINT_ADULT);

    std::unordered_map<std::string, int> riToFc;
    for (size_t i = 0; i < sComboFc.size(); i++) {
        riToFc[sComboFc[i].riName] = (int)i;
    }
    std::vector<std::string>& mmUsedNames = sMmUsedByStage;

    // OoT's spots for each restricted category — its half of each shared pool; MM contributes the
    // other half through the manifest's categoryChecks. Only consulted in Category Spots mode.
    //   songs   — the 12 RCTYPE_SONG_LOCATION checks.
    //   rewards — the 9 boss reward locations. They also sit in `bannedOot` below, which is right:
    //             that set governs the TURN LOOP, and reward spots must stay off-limits to ordinary
    //             cross-placed items. The stage here places into them directly.
    //
    // Everything is intersected with ctx->allLocations, because a location that is not in this seed's
    // pool can never come back from ReachabilitySearch — counting it as a spot would leave the stage
    // hunting for candidates that do not exist and retrying the fill forever. The counts are logged
    // so a category whose spots vanished under some option combination is visible, not silent.
    std::unordered_set<int> ootCatLocations[FC_CATEGORY_COUNT];
    // Spots that exist but never come back from ReachabilitySearch because they are not in
    // allLocations. They have no logic gate at all, so they are offered as candidates directly.
    std::unordered_set<int> ootCatAlways[FC_CATEGORY_COUNT];
    std::unordered_set<int> inPool;
    for (RandomizerCheck rc : ctx->allLocations) {
        inPool.insert((int)rc);
        auto* loc = Rando::StaticData::GetLocation(rc);
        if (loc != nullptr && loc->GetRCType() == RCTYPE_SONG_LOCATION) {
            ootCatLocations[0].insert((int)rc);
        }
    }
    for (RandomizerCheck rc : Rando::StaticData::dungeonRewardLocations) {
        if (inPool.contains((int)rc)) {
            ootCatLocations[1].insert((int)rc);
        }
    }
    // dungeonRewardLocations is only the EIGHT bosses, but OoT has NINE rewards. Vanilla puts the
    // ninth on Link's Pocket when that option asks for a dungeon reward, and on Gift From Rauru
    // otherwise (RandomizeDungeonRewards does exactly this). Mirroring it is what makes the shared
    // pool add up: 9 OoT + 4 MM remains = 13 spots for 13 items. Without the ninth the stage counted
    // 8 + 4 = 12 and refused every attempt with "more items than spots".
    //
    // Neither is in allLocations and neither has a logic requirement — Link's Pocket is literally the
    // item you start holding — so they go in the always-available set. Skijer's NEI
    ootCatAlways[1].insert(ctx->GetOption(RSK_LINKS_POCKET).Is(RO_LINKS_POCKET_DUNGEON_REWARD)
                               ? (int)RC_LINKS_POCKET
                               : (int)RC_GIFT_FROM_RAURU);

    // ELIGIBLE ROWS. The FC table here is UNFILTERED (the per-game filter runs later, on purpose),
    // so it still lists items this seed does not contain at all. Two things went wrong without this:
    //   - the count: 25 songs for 23 spots, the extras being rows neither game shuffles.
    //   - a CRASH: applying RG_TREASURE_GAME_SMALL_KEY's effect calls GetSmallKeyCount, and the
    //     Treasure Chest Game is not a dungeon, so GetDungeonFromScene returns nullptr and
    //     GetTotalSmallKeys dereferences it. With Chest Minigame off that item does not exist, and
    //     the filter always dropped it — the stage started applying it only when it stopped using
    //     the filtered table.
    // Same test the filter uses for its drop decision, and both halves are answerable this early:
    // MM's from the manifest, OoT's from its pool (untouched for these items so far). Skijer's NEI
    std::unordered_set<int> ootPoolRgs;
    for (RandomizerGet rg : itemPool) {
        ootPoolRgs.insert((int)rg);
    }
    std::vector<char> fcEligible(sComboFc.size(), 0);
    for (size_t i = 0; i < sComboFc.size(); i++) {
        fcEligible[i] = (ootPoolRgs.contains(sComboFc[i].rg) || sMmManifestPool.contains(sComboFc[i].riName)) ? 1 : 0;
    }

    // ---- 2b) RESTRICTED CATEGORIES: THEIR OWN STAGE, placed FIRST and then immovable ----
    //
    // A category's items are bound to specific spots, and those spots are among the deepest checks in
    // either game (Sheik in Crater, Sheik at Colossus, every boss room...). The turn loop CANNOT
    // place them: its assumed inventory is deliberately partial (own backlog + what the other world
    // announced), so those spots never become reachable and the fill reported "0 reachable" on every
    // single turn.
    //
    // So they get their own stage, before everything else, exactly like OoT's own
    // AssumedFill(songs, songLocations): assume the WHOLE pool minus the copy being placed, pick a
    // reachable spot of that category in either world, and place it. Afterwards the item and its spot
    // are gone from the run - the item is skipped by the split (step 3) and the spot is either
    // non-empty (OoT) or in mmUsedNames (MM), so nothing downstream can take or move them.
    //
    // This is the generic form: one pass per category in Category Spots mode. It never relaxes a
    // restriction — if an item has no reachable spot the whole attempt is retried, because placing it
    // somewhere else would silently turn the option off. Skijer's NEI
    for (int cat = 0; cat < FC_CATEGORY_COUNT; cat++) {
        if (FcCategoryMode(cat) != FC_RESTRICT_CATEGORY_SPOTS) {
            continue;
        }
        const char* catLabel = gFcCategories[cat].label;
        const std::unordered_set<int>& ootSpots = ootCatLocations[cat];
        const std::unordered_set<int>& ootAlways = ootCatAlways[cat];

        // Membership straight from the FC flags: sFcRestrict is built by BuildFcRestrictions, which
        // runs inside the per-game filter, and that has deliberately not run yet.
        //
        // The table is unfiltered, so it still holds rows NEITHER game shuffles — and those are not
        // items to deal out. Measured: the stage counted 25 songs for 23 spots and refused every
        // attempt, the two extras being RI_SONG_LULLABY and RI_SONG_LULLABY_INTRO, which the filter
        // drops. So apply that same drop test here. It is the one part of the filter that can be
        // answered this early: MM's side comes from the manifest, loaded before the fill starts, and
        // OoT's from its pool, which for a category item no native stage has touched yet.
        std::vector<int> catCopies;
        for (size_t i = 0; i < sComboFc.size(); i++) {
            bool inCat = false;
            for (int k = 0; k < FC_COMBO_ITEM_COUNT; k++) {
                if (gFcComboItems[k].fcId == sComboFc[i].fcId) {
                    inCat = (gFcComboItems[k].flags & gFcCategories[cat].itemFlag) != 0;
                    break;
                }
            }
            if (!inCat) {
                continue;
            }
            if (!fcEligible[i]) {
                SPDLOG_INFO("[FleetComboRando] '{}' stage: skipping {} - neither game shuffles it", catLabel,
                            sComboFc[i].riName);
                continue;
            }
            for (int k = 0; k < sComboFc[i].count; k++) {
                catCopies.push_back((int)i);
            }
        }
        std::shuffle(catCopies.begin(), catCopies.end(), sRng);

        std::vector<std::string> mmFree(sMmCatChecks[cat].begin(), sMmCatChecks[cat].end());
        std::shuffle(mmFree.begin(), mmFree.end(), sRng);
        // WHAT THIS STAGE PLACED AND WHERE, for whatever category runs. Generation dies before the
        // seed summary is written, so on a failure there was no way to see where a category item
        // ended up - and "Zelda's Lullaby is nowhere" had to be inferred from a location that was
        // unreachable three rooms away. This states it outright, for any category present or future.
        std::string placedLog;
        // COUNT THE FREE ONES, NOT ALL OF THEM. A spot that already holds something is not a spot:
        // excluded locations get junk before this hook runs (Fill does that early), and any native
        // stage that was not reserved out can have taken one too. Counting the raw sets made the stage
        // believe it had 23 song spots when one was already full, so it placed 22 songs and then
        // reported the 23rd as "no reachable spot" — a completely misleading diagnosis of an
        // off-by-one in its own budget. Skijer's NEI
        size_t ootFree = 0;
        std::string takenList;
        auto countFree = [&](const std::unordered_set<int>& spots) {
            for (int rcInt : spots) {
                auto* loc = ctx->GetItemLocation((RandomizerCheck)rcInt);
                if (loc != nullptr && loc->GetPlacedRandomizerGet() == RG_NONE) {
                    ootFree++;
                } else {
                    auto* staticLoc = Rando::StaticData::GetLocation((RandomizerCheck)rcInt);
                    takenList += takenList.empty() ? "" : ", ";
                    takenList += staticLoc != nullptr ? staticLoc->GetName() : "?";
                }
            }
        };
        countFree(ootSpots);
        countFree(ootAlways);
        SPDLOG_INFO("[FleetComboRando] '{}' stage: {} copies into {} free OoT (of {} logic + {} always) + {} MM spots",
                    catLabel, catCopies.size(), ootFree, ootSpots.size(), ootAlways.size(), mmFree.size());
        if (!takenList.empty()) {
            // Not fatal on its own (MM may have room), but it is always worth knowing: these were
            // supposed to be reserved for this category and something got there first.
            SPDLOG_WARN("[FleetComboRando] '{}' stage: {} OoT spot(s) already taken before the stage: {}", catLabel,
                        ootSpots.size() + ootAlways.size() - ootFree, takenList);
        }

        // More items than spots can never work, and retrying cannot fix it — the counts do not depend
        // on the RNG. Say so once and give up instead of spinning through 25 attempts. Skijer's NEI
        if (catCopies.size() > ootFree + mmFree.size()) {
            SPDLOG_ERROR("[FleetComboRando] '{}' stage: {} items but only {} FREE spots ({} OoT + {} MM) - "
                         "this option combination cannot be satisfied",
                         catLabel, catCopies.size(), ootFree + mmFree.size(), ootFree, mmFree.size());
            SetStatus(std::string("Shared ") + catLabel + ": more items than free spots - check the option");
            return false;
        }

        for (int fcIdx : catCopies) {
            // Assume everything EXCEPT this copy - the guarantee that makes the spot reachable
            // without the item it is about to hold.
            logic->Reset();
            for (size_t i = 0; i < sComboFc.size(); i++) {
                if (!fcEligible[i]) {
                    continue; // not in this seed at all - and applying some of them crashes
                }
                int have = sComboFc[i].count - ((int)i == fcIdx ? 1 : 0);
                for (int k = 0; k < have; k++) {
                    Rando::StaticData::RetrieveItem((RandomizerGet)sComboFc[i].rg).ApplyEffect();
                }
            }
            for (RandomizerGet rg : itemPool) {
                if (Rando::StaticData::RetrieveItem(rg).IsAdvancement()) {
                    Rando::StaticData::RetrieveItem(rg).ApplyEffect();
                }
            }

            std::vector<int> ootCands;
            for (RandomizerCheck rc : ReachabilitySearch(searchLocations)) {
                if (!ootSpots.contains((int)rc)) {
                    continue;
                }
                auto* loc = ctx->GetItemLocation(rc);
                if (loc != nullptr && loc->GetPlacedRandomizerGet() == RG_NONE) {
                    ootCands.push_back((int)rc);
                }
            }
            for (int rcInt : ootAlways) {
                auto* loc = ctx->GetItemLocation((RandomizerCheck)rcInt);
                if (loc != nullptr && loc->GetPlacedRandomizerGet() == RG_NONE) {
                    ootCands.push_back(rcInt);
                }
            }

            // MM's side of the pool: ask the oracle what it can reach with the same assumption.
            std::vector<std::string> mmCands;
            if (!mmFree.empty() && !sComboFc[fcIdx].riName.empty() && sComboFc[fcIdx].riName != "FCI_NO_ITEM") {
                std::vector<std::pair<int, int>> fcItems;
                for (size_t i = 0; i < sComboFc.size(); i++) {
                    if (!fcEligible[i]) {
                        continue;
                    }
                    int have = sComboFc[i].count - ((int)i == fcIdx ? 1 : 0);
                    if (have > 0) {
                        fcItems.push_back({ sComboFc[i].fcId, have });
                    }
                }
                std::map<std::string, int> mmAgg;
                for (auto& name : sMmPoolProg) {
                    if (!riToFc.contains(name)) {
                        mmAgg[name]++;
                    }
                }
                std::vector<std::pair<std::string, int>> mmItems(mmAgg.begin(), mmAgg.end());
                nlohmann::json resp =
                    OracleBlocking(FleetOracle_SendReachableRequest(fcItems, mmItems), "reachable", 45000);
                std::set<std::string> reachNames;
                if (resp.contains("reachable") && resp["reachable"].is_array()) {
                    for (auto& idJson : resp["reachable"]) {
                        auto it = sMmCheckName.find(idJson.get<int>());
                        if (it != sMmCheckName.end()) {
                            reachNames.insert(it->second);
                        }
                    }
                }
                for (auto& sc : mmFree) {
                    if (reachNames.contains(sc)) {
                        mmCands.push_back(sc);
                    }
                }
            }

            size_t total = ootCands.size() + mmCands.size();
            if (total == 0) {
                // PER-SPOT DUMP. "0 of 8 reachable" does not say WHICH spot was left over nor whether
                // the problem was occupancy or reachability, and those need opposite fixes. Print the
                // state of every spot in the category so the answer is in the log the first time.
                std::set<int> reachSet;
                for (RandomizerCheck rc : ReachabilitySearch(searchLocations)) {
                    reachSet.insert((int)rc);
                }
                std::string dump;
                auto describe = [&](int rcInt, const char* kind) {
                    auto* loc = ctx->GetItemLocation((RandomizerCheck)rcInt);
                    auto* staticLoc = Rando::StaticData::GetLocation((RandomizerCheck)rcInt);
                    bool free = loc != nullptr && loc->GetPlacedRandomizerGet() == RG_NONE;
                    dump += "\n    [";
                    dump += kind;
                    dump += "] ";
                    dump += staticLoc != nullptr ? staticLoc->GetName() : "?";
                    // Reachability is only MEANINGFUL for a free spot: ReachabilitySearch never
                    // returns a location that already holds something, so printing "NOT-reachable"
                    // next to a taken one invents a second problem that is not there. Skijer's NEI
                    dump +=
                        free ? (reachSet.contains(rcInt) ? " : FREE reachable" : " : FREE not-reachable") : " : taken";
                };
                for (int rcInt : ootSpots) {
                    describe(rcInt, "oot");
                }
                for (int rcInt : ootAlways) {
                    describe(rcInt, "always");
                }
                // THE NUMBER THAT DECIDES. The assumed inventory here is "the whole pool minus this
                // one copy", so almost every location in the game should come back reachable. If this
                // count is a few dozen, the assumption is not reaching the logic and the bug is in
                // how the inventory is applied; if it is in the thousands, the leftover spot is
                // behind a genuine gate and the fix belongs in the placement order. Without it the
                // two are indistinguishable from the outside. Skijer's NEI
                SPDLOG_ERROR("[FleetComboRando] '{}' stage: '{}' has no reachable spot "
                             "(OoT {} logic + {} always, MM free {}; search reached {} of {} OoT "
                             "locations) - retrying{}",
                             catLabel, sComboFc[fcIdx].riName, ootSpots.size(), ootAlways.size(), mmFree.size(),
                             reachSet.size(), ctx->allLocations.size(), dump);
                SetStatus(std::string("Delegated fill: a ") + catLabel + " item has no reachable spot - retrying");
                return false;
            }
            size_t pick = std::uniform_int_distribution<size_t>(0, total - 1)(sRng);
            if (pick < ootCands.size()) {
                ctx->PlaceItemInLocation((RandomizerCheck)ootCands[pick], (RandomizerGet)sComboFc[fcIdx].rg);
                sStagePlacedOot.push_back({ ootCands[pick], sComboFc[fcIdx].rg });
                auto* staticLoc = Rando::StaticData::GetLocation((RandomizerCheck)ootCands[pick]);
                placedLog += placedLog.empty() ? "" : ", ";
                placedLog += sComboFc[fcIdx].riName;
                placedLog += " -> OoT ";
                placedLog += staticLoc != nullptr ? staticLoc->GetName() : "?";
            } else {
                const std::string& check = mmCands[pick - ootCands.size()];
                sMmPlacements[check] = sComboFc[fcIdx].riName;
                mmUsedNames.push_back(check);
                placedLog += placedLog.empty() ? "" : ", ";
                placedLog += sComboFc[fcIdx].riName;
                placedLog += " -> MM ";
                placedLog += check;
                // The RG, not the index: the per-game filter rebuilds sComboFc afterwards and every
                // index shifts. Storing the index here injected whatever row landed in that slot.
                sStagePlacedMm.push_back({ check, sComboFc[fcIdx].rg });
                mmFree.erase(std::remove(mmFree.begin(), mmFree.end(), check), mmFree.end());
            }
        }
        SPDLOG_INFO("[FleetComboRando] '{}' stage OK: {} items placed\n    {}", catLabel, catCopies.size(), placedLog);
    }

    return true;
}

bool RunDelegatedFill() {
    auto ctx = Rando::Context::GetInstance();
    sLocalOnlyMm.clear(); // rebuilt from scratch on every attempt
    // The per-game filter runs HERE and nowhere else, on purpose. It decides whether an item is
    // shuffled in its own game by reading OoT's itemPool, and that reading is only correct once the
    // native restricted stages have removed what they own — own-dungeon keys, boss keys, maps,
    // compasses, key rings. That is what keeps them out of the shared pool.
    ApplyFcPerGameFilter();

    // ALREADY PLACED BY A NATIVE RESTRICTED STAGE. Fill() runs its own restricted stages BEFORE this
    // hook (own-dungeon items, dungeon rewards, and RandomizeLinksPocket), and those stages take
    // their item straight out of the pool. The shared pool is built from the FC table, which knows
    // nothing about that — so a reward Link's Pocket had already consumed got a SECOND copy placed
    // cross-game. Measured on four seeds in a row: exactly one duplicated dungeon reward each
    // (Kokiri's Emerald, Fire Medallion, Light Medallion, Zora's Sapphire), always with the first
    // copy sitting in Link's Pocket.
    //
    // Same rule the song stage follows: what is already placed is out of the pool. Counting the
    // copies here and subtracting them below is the generic form of it, so any future restricted
    // stage (dungeon rewards, MM remains) is covered without touching this again. Skijer's NEI
    std::unordered_map<int, int> prePlacedRg;
    for (RandomizerCheck rc : ctx->allLocations) {
        auto* loc = ctx->GetItemLocation(rc);
        if (loc != nullptr && loc->GetPlacedRandomizerGet() != RG_NONE) {
            prePlacedRg[(int)loc->GetPlacedRandomizerGet()]++;
        }
    }

    // 1) dedupe SoH's native itemPool: drop ALL copies of the RGs covered by FC.
    // NOTE: the noCross ones (vanilla / start-with / end-of-dungeon) are NOT pre-placed, so they must
    // STAY in the native pool - erasing them here without placing them would lose them entirely.
    std::unordered_set<int> fcRgs;
    for (size_t i = 0; i < sComboFc.size(); i++) {
        if (i < sFcRestrict.size() && sFcRestrict[i].noCross) {
            continue;
        }
        fcRgs.insert(sComboFc[i].rg);
    }
    std::erase_if(itemPool, [&](RandomizerGet rg) { return fcRgs.contains((int)rg); });

    // 2) locations OoT vetadas para la pre-colocación (etapas nativas las necesitan libres)
    std::unordered_set<int> bannedOot;
    for (RandomizerCheck rc : Rando::StaticData::dungeonRewardLocations) {
        bannedOot.insert((int)rc);
    }
    bannedOot.insert((int)RC_LINKS_POCKET);
    bannedOot.insert((int)RC_GIFT_FROM_RAURU);
    // Centinelas del PORTAL (no son item locations): se consultan en la búsqueda pero jamás se
    // usan como candidatos de colocación.
    bannedOot.insert((int)RC_ALTAR_HINT_CHILD);
    bannedOot.insert((int)RC_ALTAR_HINT_ADULT);

    // GATE DEL PORTAL: MM solo es alcanzable si la lógica de OoT puede llegar DENTRO del Temple of
    // Time (donde vive el fleet hole). Centinela = los checks de altar de RR_TEMPLE_OF_TIME
    // (child o adult adentro). Se añaden a la lista de búsqueda para poder consultarlos.
    std::vector<RandomizerCheck> searchLocations = ctx->allLocations;
    searchLocations.push_back(RC_ALTAR_HINT_CHILD);
    searchLocations.push_back(RC_ALTAR_HINT_ADULT);

    // Seeded with the MM checks the restricted stages already claimed, so the turn loop can never
    // hand one of them out twice.
    std::vector<std::string> mmUsedNames(sMmUsedByStage.begin(), sMmUsedByStage.end());
    std::unordered_map<std::string, int> riToFc;
    for (size_t i = 0; i < sComboFc.size(); i++) {
        riToFc[sComboFc[i].riName] = (int)i;
    }

    // ---- 3) SPLIT: which GAME gets each shared item (never which slot) ----
    // Where an item lands is its host game's own fill's job; the combo only decides the side. Seeded
    // from sRng, so the same seed always produces the same split. Copies of the same item are diced
    // independently so both worlds stay equally relevant instead of one hoarding a whole chain.
    std::vector<int> fcToOot(sComboFc.size(), 0);
    std::vector<int> fcToMm(sComboFc.size(), 0);

    // No spot-owner pool here any more: a restricted category is dealt across both worlds' spots by
    // its own stage above (which is also the only place that can guarantee "reachable without the
    // item it holds"), so by the time the split runs those items are already placed and skipped.
    for (size_t i = 0; i < sComboFc.size(); i++) {
        if (i < sFcRestrict.size() && sFcRestrict[i].noCross) {
            continue; // bound to OoT by its own option; a native stage already placed it
        }
        // Restricted-category items were already placed by the stage above and are immovable: they
        // are out of the pool AND their spots are out of the available set. Skijer's NEI
        if (i < sFcRestrict.size() && sFcRestrict[i].restrictCat >= 0) {
            continue;
        }
        // A world only gets copies of what it actually shuffles; otherwise its native path already
        // supplies the item and a cross copy would be a duplicate. When only one side is eligible it
        // takes every copy — the item still crosses, it just cannot land in the world that has it
        // nailed down. Skijer's NEI
        bool eligibleOot = sComboFc[i].inOot;
        bool eligibleMm = sComboFc[i].inMm;
        // MM's own-dungeon / start-with placement for ITS keys and fairies: never to OoT. If MM does
        // not even pool them (start-with: it grants them at file creation), nobody supplies copies.
        if (i < sFcRestrict.size() && sFcRestrict[i].pinMm) {
            eligibleOot = false;
            if (!eligibleMm) {
                continue; // MM starts with them (or doesn't shuffle them): nothing to deal out
            }
        }
        // Copies a native restricted stage already handed out are gone from the pool, so the shared
        // pool must supply that many fewer. Without this the seed ends up with two Kokiri's Emeralds.
        auto preIt = prePlacedRg.find(sComboFc[i].rg);
        int effCount = sComboFc[i].count - (preIt != prePlacedRg.end() ? preIt->second : 0);
        if (effCount < sComboFc[i].count) {
            SPDLOG_INFO("[FleetComboRando] {}: {} of {} copies already placed by a native stage, "
                        "cross-placing {}",
                        sComboFc[i].riName, sComboFc[i].count - effCount, sComboFc[i].count, std::max(0, effCount));
        }
        for (int k = 0; k < effCount; k++) {
            if (!eligibleOot) {
                fcToMm[i]++;
            } else if (!eligibleMm) {
                fcToOot[i]++;
            } else if (std::uniform_int_distribution<int>(0, 1)(sRng) == 1) {
                fcToMm[i]++;
            } else {
                fcToOot[i]++;
            }
        }
    }

    // SUPPLY CHECK, before anything is placed. A chained item that comes up SHORT is invisible until
    // some door deep in the seed refuses to open: two Strength Upgrades instead of three reads as
    // "Golden Gauntlets do not exist", which surfaces as Ganon's Tower being unreachable and nothing
    // whatsoever pointing at strength. The seed summary has always validated this, but only on
    // success — exactly when it does not matter. Skijer's NEI
    for (size_t i = 0; i < sComboFc.size(); i++) {
        if (i < sFcRestrict.size() && (sFcRestrict[i].noCross || sFcRestrict[i].restrictCat >= 0)) {
            continue; // placed elsewhere by design; counting them here would report false shortfalls
        }
        if (i < sFcRestrict.size() && sFcRestrict[i].pinMm && !sComboFc[i].inMm) {
            continue; // MM starts with these (its own placement option); no copies are owed
        }
        int supplied = fcToOot[i] + fcToMm[i];
        auto preIt = prePlacedRg.find(sComboFc[i].rg);
        int prePlaced = preIt != prePlacedRg.end() ? preIt->second : 0;
        if (supplied + prePlaced < sComboFc[i].count) {
            SPDLOG_WARN("[FleetComboRando] SHORT SUPPLY: {} needs {} copies, only {} exist ({} cross-placed"
                        " + {} already in the world) - anything gated on the full chain is unreachable",
                        sComboFc[i].riName, sComboFc[i].count, supplied + prePlaced, supplied, prePlaced);
        }
    }

    // ---- 4) turn state ----
    std::vector<std::pair<int, std::string>> placedMmFcNames; // (fcIdx, RC name) -> StartingInventory
    std::vector<std::string> mmPending;                       // RI names still owed to MM
    std::vector<int> ootPending;                              // one fcIdx per copy still owed to OoT

    for (size_t i = 0; i < sComboFc.size(); i++) {
        for (int k = 0; k < fcToMm[i]; k++) {
            mmPending.push_back(sComboFc[i].riName);
        }
        for (int k = 0; k < fcToOot[i]; k++) {
            ootPending.push_back((int)i);
        }
    }
    // Reverse lookup RI name -> fcIdx. Needed here to keep MM's native pool from re-supplying items
    // the FC split already covers, and again later to spot which MM placements were shared items.

    // MM's OWN progression is no longer placed from here: it goes into MM's turn so 2ship picks the
    // slots with its own shuffled checkPool, which is what makes MM's options finally count.
    //
    // Skip anything the FC split already covers. ParseManifest's dedupe only drops a name when
    // FcCombo_NativeForItem() != FCI_NO_ITEM, so shared items that fail that guard stayed in
    // sMmPoolProg AND got split as FC copies — supplied twice. That is where the overshoot came from
    // (Gold Skulltula Token placed 144 times against expected=100, Progressive Ocarina 4 vs 2), and
    // those extra items ate the OoT slots the native stages needed afterwards. Skijer's NEI
    for (auto& name : sMmPoolProg) {
        if (riToFc.contains(name)) {
            continue;
        }
        mmPending.push_back(name);
    }
    std::shuffle(ootPending.begin(), ootPending.end(), sRng);
    std::shuffle(mmPending.begin(), mmPending.end(), sRng);

    // Nothing has to be ordered last any more. Restricted-category items used to be dealt through the
    // turn loop and had to go at the very end, because their spots are deep in the logic and almost
    // none are reachable early — attempted first they stalled both worlds and read as a deadlock.
    // They now have their own stage before the split, so the turn loop never sees them. Skijer's NEI

    const int totalToPlace = (int)(ootPending.size() + mmPending.size());
    int placedCount = 0;

    // ANNOUNCED-ITEMS MODEL. Neither side is allowed to assume the shared pool wholesale. A world may
    // assume exactly two things:
    //   a) its OWN backlog — the copies it still has to place itself (ordinary assumed fill), and
    //   b) what the OTHER world has explicitly ANNOUNCED it already placed.
    // Blanket assumption is what broke every seed: OoT assumed the copies destined for Termina, so it
    // happily placed into slots gated behind items it could never collect, and every attempt came back
    // `playthroughBeatable=false`. An announcement is a fact (the item is down, in a slot the announcer
    // could reach at that moment), not a promise. Skijer's NEI
    std::vector<int> ootBacklog(sComboFc.size(), 0);     // copies still owed to OoT, by fcIdx
    std::vector<int> announcedToOot(sComboFc.size(), 0); // copies MM has told us it placed
    for (size_t i = 0; i < sComboFc.size(); i++) {
        ootBacklog[i] = fcToOot[i];
    }

    // OoT reachability with the CURRENT assumed inventory: its own unplaced backlog, plus MM's
    // announcements, plus OoT's own advancement pool. ReachabilitySearch collects whatever is already
    // placed and within reach on OoT's side; MM's side is unreachable to that search, which is exactly
    // why announcements have to be applied by hand here.
    //
    // The item about to be placed is removed from `ootBacklog` BEFORE calling this, so it can never
    // be used to justify reaching its own location. Skipping that is what produced seeds where
    // everything landed in one turn and nothing was beatable. Skijer's NEI
    auto ootReachableNow = [&]() {
        logic->Reset();
        for (size_t i = 0; i < sComboFc.size(); i++) {
            int have = ootBacklog[i] + announcedToOot[i];
            for (int k = 0; k < have; k++) {
                Rando::StaticData::RetrieveItem((RandomizerGet)sComboFc[i].rg).ApplyEffect();
            }
        }
        for (RandomizerGet rg : itemPool) {
            if (Rando::StaticData::RetrieveItem(rg).IsAdvancement()) {
                Rando::StaticData::RetrieveItem(rg).ApplyEffect();
            }
        }
        std::set<int> out;
        for (RandomizerCheck rc : ReachabilitySearch(searchLocations)) {
            out.insert((int)rc);
        }
        return out;
    };

    // What OoT has announced to MM, as MM item names. Starts EMPTY: MM may not assume a single OoT
    // item until OoT says it put one down. Grows by one entry per OoT placement of a shared item.
    std::map<std::string, int> announcedToMm;
    auto assumedForMmVec = [&]() {
        return std::vector<std::pair<std::string, int>>(announcedToMm.begin(), announcedToMm.end());
    };

    // Pre-placed spots already announced, so each is announced exactly once. Keyed by RG because the
    // per-game filter rebuilds sComboFc after the stages run and every index shifts.
    std::set<int> stageAnnounced;           // OoT-side spots, by RandomizerCheck
    std::set<std::string> stageAnnouncedMm; // MM-side spots, by check name
    std::unordered_map<int, int> rgToFcIdxTurn;
    for (size_t i = 0; i < sComboFc.size(); i++) {
        rgToFcIdxTurn[sComboFc[i].rg] = (int)i;
    }

    // ---- 5) TURN LOOP: each game fills itself, sphere by sphere ----
    // One turn = the live world places, with ITS OWN logic, into ITS OWN reachable slots, until it is
    // STUCK — meaning either no items left to place or no reachable slot for the next one. Then it
    // ANNOUNCES what it just put down and cedes to the other world, which believes those items and
    // nothing else, and does the same. A world that must place an item absent from the shared pool
    // keeps it at home and it is logged to the .fleet. Deadlock is both sides ceding having placed
    // nothing. Deterministic: no timing, no polling, everything seeded. Skijer's NEI
    const uint32_t seedBase = (uint32_t)ctx->GetSeed();
    int deadTurns = 0;

    // THE PORTAL IS ONE BIDIRECTIONAL EDGE: Temple of Time (OoT) <-> South Clock Town (MM). Whichever
    // world the seed starts in is the one that is live at turn 1; the other stays closed until the
    // live one can stand at its end of the edge. Starting in MM is symmetric, not a special case:
    // OoT is not considered until MM's logic reaches South Clock Town.
    const bool startInMm = CVarGetInteger("gFleetCombo.StartInMM", 0) != 0;
    bool ootOpen = !startInMm;
    bool mmOpen = startInMm;
    SPDLOG_INFO("[FleetComboRando] delegated fill starts in {}", startInMm ? "MM" : "OoT");

    // LOANS. While one world is still closed the other may be starved: the blind split can hand it
    // items it needs to even reach the portal. The deadlock handler therefore lends the closed world's
    // share to the live one — but a loan that is never called back means the live world places
    // EVERYTHING and the other ends up with nothing but junk (seed 790065491: "moved all 237 items
    // MM -> OoT", then 465/465 in OoT and 0 shared items in Termina). So the loan is repaid the instant
    // the portal opens: whatever is still unplaced goes home. Skijer's NEI
    std::vector<int> loanToOot; // fcIdx lent from MM's share to OoT
    std::vector<int> loanToMm;  // fcIdx lent from OoT's share to MM

    auto RepayLoanToMm = [&]() {
        size_t back = 0;
        for (int fcIdx : loanToOot) {
            auto it = std::find(ootPending.begin(), ootPending.end(), fcIdx);
            if (it == ootPending.end()) {
                continue; // already placed in OoT; the loan on that copy is settled
            }
            ootPending.erase(it);
            ootBacklog[fcIdx]--;
            mmPending.push_back(sComboFc[fcIdx].riName);
            back++;
        }
        loanToOot.clear();
        if (back > 0) {
            SPDLOG_INFO("[FleetComboRando] portal open: repaid {} unplaced items back to MM", back);
        }
    };
    auto RepayLoanToOot = [&]() {
        size_t back = 0;
        for (int fcIdx : loanToMm) {
            auto it = std::find(mmPending.begin(), mmPending.end(), sComboFc[fcIdx].riName);
            if (it == mmPending.end()) {
                continue; // already placed in MM
            }
            mmPending.erase(it);
            ootPending.push_back(fcIdx);
            ootBacklog[fcIdx]++;
            back++;
        }
        loanToMm.clear();
        if (back > 0) {
            SPDLOG_INFO("[FleetComboRando] portal open: repaid {} unplaced items back to OoT", back);
        }
    };

    for (int turn = 1; turn <= 64 && (!ootPending.empty() || !mmPending.empty()); turn++) {
        bool progressed = false;

        // --- OoT's turn: assumed fill, ONE ITEM AT A TIME ---
        // Per item: drop it from the assumed inventory, recompute what is reachable WITHOUT it, and
        // only then pick a slot. That guarantee ("the location is reachable without the item it
        // holds") is what makes the seed beatable; batching a whole turn against one reachability
        // snapshot silently breaks it. ReachabilitySearch is native and local, so the cost is fine —
        // the expensive round-trips were always the oracle ones, and those stay batched per turn.
        if (ootOpen && !ootPending.empty()) {
            size_t placedThisTurn = 0;
            while (!ootPending.empty()) {
                int fcIdx = ootPending.front();
                ootBacklog[fcIdx]--; // assume our own backlog EXCEPT the copy being placed right now

                std::set<int> reach = ootReachableNow();

                // ANNOUNCE PRE-PLACED SPOTS ON ARRIVAL, not up front. The restricted stages filled
                // ~36 spots before this race started; those items are only facts once somebody can
                // stand on the spot holding them. So the moment OoT's reachability covers one, MM is
                // told — same rule as any other placement, just discovered instead of made.
                // OoT itself needs no such step: its own search applies whatever it walks over.
                for (auto& [rcInt, rgInt] : sStagePlacedOot) {
                    if (stageAnnounced.contains(rcInt) || !reach.contains(rcInt)) {
                        continue;
                    }
                    auto fcIt = rgToFcIdxTurn.find(rgInt);
                    if (fcIt == rgToFcIdxTurn.end() || sComboFc[fcIt->second].riName.empty() ||
                        sComboFc[fcIt->second].riName == "FCI_NO_ITEM") {
                        continue;
                    }
                    stageAnnounced.insert(rcInt);
                    announcedToMm[sComboFc[fcIt->second].riName]++;
                }

                if (!mmOpen && (reach.contains((int)RC_ALTAR_HINT_CHILD) || reach.contains((int)RC_ALTAR_HINT_ADULT))) {
                    mmOpen = true; // OoT logic can stand inside the Temple of Time -> Termina opens
                    SPDLOG_INFO("[FleetComboRando] turn {}: OoT reached the portal, MM joins the fill", turn);
                    ootBacklog[fcIdx]++; // undo the trial decrement; nothing was placed
                    RepayLoanToMm();
                    break; // CEDE IMMEDIATELY: the whole point of turns is that MM reacts to this
                }

                // No category filter here: restricted-category items never reach the turn loop, the
                // stage before the split already placed them. Their spots are equally out of reach —
                // OoT's are non-empty, and `bannedOot` keeps the reward locations off the table.
                std::vector<int> cands;
                for (int rcInt : reach) {
                    if (bannedOot.contains(rcInt)) {
                        continue;
                    }
                    auto* loc = ctx->GetItemLocation((RandomizerCheck)rcInt);
                    if (loc != nullptr && loc->GetPlacedRandomizerGet() == RG_NONE) {
                        cands.push_back(rcInt);
                    }
                }
                if (cands.empty()) {
                    ootBacklog[fcIdx]++; // put it back; MM's turn may open something up
                    break;               // STUCK: no reachable candidate -> cede the turn
                }
                int pick = cands[std::uniform_int_distribution<size_t>(0, cands.size() - 1)(sRng)];
                ctx->PlaceItemInLocation((RandomizerCheck)pick, (RandomizerGet)sComboFc[fcIdx].rg);
                ootPending.erase(ootPending.begin());
                // ANNOUNCE it to MM: the copy is down, in a slot OoT could reach without it, so MM is
                // entitled to assume it from now on — and only from now on.
                if (!sComboFc[fcIdx].riName.empty() && sComboFc[fcIdx].riName != "FCI_NO_ITEM") {
                    announcedToMm[sComboFc[fcIdx].riName]++;
                }
                placedThisTurn++;
            }
            if (placedThisTurn > 0) {
                placedCount += (int)placedThisTurn;
                progressed = true;
            }
        }

        // --- MM's turn (only once MM's side of the portal is live) ---
        if (mmOpen && !mmPending.empty()) {
            std::map<std::string, int> agg;
            for (auto& name : mmPending) {
                agg[name]++;
            }
            std::vector<std::pair<std::string, int>> toPlace(agg.begin(), agg.end());

            nlohmann::json resp = OracleBlocking(
                FleetOracle_SendFillTurnRequest(assumedForMmVec(), toPlace, mmUsedNames, seedBase + (uint32_t)turn),
                "fillTurn", 45000);

            // The other end of the edge: MM standing in South Clock Town unlocks OoT. This is what
            // makes "start in MM" work rather than deadlocking with OoT permanently closed.
            if (!ootOpen && resp.value("portalReachable", false)) {
                ootOpen = true;
                SPDLOG_INFO("[FleetComboRando] turn {}: MM reached the portal, OoT joins the fill", turn);
                RepayLoanToOot();
            }

            int placedThisTurn = 0;
            if (resp.contains("placed") && resp["placed"].is_object()) {
                for (auto& [rcName, riName] : resp["placed"].items()) {
                    sMmPlacements[rcName] = riName.get<std::string>();
                    mmUsedNames.push_back(rcName);
                    auto it = riToFc.find(riName.get<std::string>());
                    if (it != riToFc.end()) {
                        placedMmFcNames.push_back({ it->second, rcName });
                        // MM'S ANNOUNCEMENT BACK TO OoT. The copy is down in Termina, in a slot MM
                        // could reach, so from now on OoT may assume it — it is collectable by walking
                        // through the portal. Before this announcement OoT assumed nothing about it,
                        // which is the whole difference from the old blanket model. Step 6 turns these
                        // same entries into StartingInventory for the native fill. Skijer's NEI
                        announcedToOot[it->second]++;
                    } else {
                        // Not in the shared pool: MM had to keep it at home. Recorded so a new upstream
                        // item never goes unnoticed. Skijer's NEI
                        sLocalOnlyMm.push_back(riName.get<std::string>() + " @ " + rcName);
                    }
                    placedThisTurn++;
                }
            }
            // MM REACHED A PRE-PLACED SPOT. Same announcement, for the items the restricted stages
            // dropped into Termina before the race: MM says which of those checks it actually stood
            // on this turn, and only then may OoT assume what is in them. The mirror of what OoT does
            // for its own pre-placed spots above. Skijer's NEI
            if (resp.contains("prePlacedReached") && resp["prePlacedReached"].is_array()) {
                for (auto& checkJson : resp["prePlacedReached"]) {
                    std::string check = checkJson.get<std::string>();
                    if (stageAnnouncedMm.contains(check)) {
                        continue;
                    }
                    for (auto& [stageCheck, rg] : sStagePlacedMm) {
                        if (stageCheck != check) {
                            continue;
                        }
                        auto fcIt = rgToFcIdxTurn.find(rg);
                        if (fcIt != rgToFcIdxTurn.end()) {
                            stageAnnouncedMm.insert(check);
                            announcedToOot[fcIt->second]++;
                        }
                        break;
                    }
                }
            }

            // `remaining` is authoritative: MM tells us what it could not fit this turn.
            mmPending.clear();
            if (resp.contains("remaining") && resp["remaining"].is_array()) {
                for (auto& n : resp["remaining"]) {
                    mmPending.push_back(n.get<std::string>());
                }
            }
            if (placedThisTurn > 0) {
                placedCount += placedThisTurn;
                progressed = true;
            }
        }

        SetStatus("Delegated fill turn " + std::to_string(turn) + ": " + std::to_string(placedCount) + "/" +
                  std::to_string(totalToPlace) + " placed (" + std::to_string(ootPending.size()) + " OoT, " +
                  std::to_string(mmPending.size()) + " MM pending)");

        if (progressed) {
            deadTurns = 0;
            continue;
        }

        // --- Deadlock: neither side could place anything ---
        // Re-deal the stuck items to the other side rather than throwing the whole attempt away: a
        // shared item is only stuck because THIS world has no room or no reach for it, and the other
        // world usually does. Seeded, so a rerun of the same seed deadlocks and recovers identically.
        //
        // STARVED STARTING WORLD. Under the announced model the live world may only assume its own
        // share, so a blind 50/50 split can hand the items it needs to reach the portal to a world
        // that is not even open yet — stuck on turn 1 with no way to ever unstick. When the other side
        // is still closed the re-deal is therefore TOTAL, not a half slice: every shared copy goes to
        // the world that is actually playing. Skijer's NEI
        // (Songs are not handled here any more: the restricted stage placed them all before the
        //  split, so they never enter these queues and can never be the reason for a stall.)

        deadTurns++;
        if (deadTurns > 3) {
            SetStatus("Delegated fill: deadlocked with " + std::to_string(ootPending.size()) + " OoT and " +
                      std::to_string(mmPending.size()) + " MM items left - retrying");
            return false;
        }
        if (!ootOpen && !ootPending.empty()) {
            // MM is live and OoT is not: pull every shared copy out of OoT's share into MM's.
            size_t moved = 0;
            std::vector<int> keepOot;
            for (int fcIdx : ootPending) {
                if (sComboFc[fcIdx].riName.empty() || sComboFc[fcIdx].riName == "FCI_NO_ITEM" ||
                    !sComboFc[fcIdx].inMm) {
                    keepOot.push_back(fcIdx); // OoT-only, or MM does not shuffle it: it cannot cross
                    continue;
                }
                mmPending.push_back(sComboFc[fcIdx].riName);
                ootBacklog[fcIdx]--;
                loanToMm.push_back(fcIdx); // called back the moment OoT opens
                moved++;
            }
            ootPending = std::move(keepOot);
            SPDLOG_INFO("[FleetComboRando] turn {}: OoT still closed, lent all {} items OoT -> MM", turn, moved);
            if (moved == 0) {
                return false; // MM cannot reach the portal and has nothing left to try
            }
        } else if (!mmOpen && !mmPending.empty()) {
            // OoT is live and MM is not: pull every shared copy out of MM's share into OoT's.
            size_t moved = 0;
            std::vector<std::string> keep;
            for (auto& name : mmPending) {
                auto it = riToFc.find(name);
                if (it != riToFc.end() && sComboFc[it->second].inOot) {
                    ootPending.push_back(it->second);
                    ootBacklog[it->second]++;
                    loanToOot.push_back(it->second); // called back the moment MM opens
                    moved++;
                } else {
                    keep.push_back(name); // MM-native, or OoT does not shuffle it: it cannot cross
                }
            }
            mmPending = std::move(keep);
            SPDLOG_INFO("[FleetComboRando] turn {}: MM still closed, lent all {} items MM -> OoT", turn, moved);
            if (moved == 0) {
                return false; // OoT cannot reach the portal and has nothing left to try
            }
        } else if (!ootPending.empty() && mmOpen) {
            // Hand a slice of OoT's backlog to MM (only what MM is eligible to receive).
            size_t want = (ootPending.size() + 1) / 2;
            size_t moved = 0;
            std::vector<int> keepOot;
            for (size_t j = ootPending.size(); j-- > 0;) {
                int fcIdx = ootPending[j];
                if (moved < want && sComboFc[fcIdx].inMm && !sComboFc[fcIdx].riName.empty() &&
                    sComboFc[fcIdx].riName != "FCI_NO_ITEM") {
                    mmPending.push_back(sComboFc[fcIdx].riName);
                    ootBacklog[fcIdx]--; // no longer OoT's to assume: it is MM's problem now
                    moved++;
                } else {
                    keepOot.push_back(fcIdx);
                }
            }
            ootPending = std::move(keepOot);
            SPDLOG_INFO("[FleetComboRando] turn {}: deadlock, moved {} items OoT -> MM", turn, moved);
            if (moved == 0) {
                return false; // nothing OoT holds may cross; the split has to be rerolled
            }
        } else if (!mmPending.empty()) {
            // Hand MM's shared backlog back to OoT (MM-native names cannot cross, they stay).
            size_t moved = 0;
            std::vector<std::string> keep;
            for (auto& name : mmPending) {
                auto it = riToFc.find(name);
                if (it != riToFc.end() && sComboFc[it->second].inOot && moved < (mmPending.size() + 1) / 2) {
                    ootPending.push_back(it->second);
                    ootBacklog[it->second]++; // OoT owes it now, so OoT may assume it again
                    moved++;
                } else {
                    keep.push_back(name);
                }
            }
            mmPending = std::move(keep);
            SPDLOG_INFO("[FleetComboRando] turn {}: deadlock, moved {} items MM -> OoT", turn, moved);
            if (moved == 0) {
                return false; // only MM-native items left and MM cannot place them: reroll the attempt
            }
        } else {
            return false;
        }
    }

    if (!ootPending.empty() || !mmPending.empty()) {
        SetStatus("Delegated fill: ran out of turns with " + std::to_string(ootPending.size() + mmPending.size()) +
                  " items left - retrying");
        return false;
    }

    // ---- 6) shared items that landed in MM feed OoT's LOGICAL StartingInventory ----
    // Sound because the turn loop only ever placed them into slots reachable at that moment, so the
    // native fill assuming them obtainable matches what a player can actually do.
    //
    // NAMED, not counted. "15 items fed" cannot answer "is item X among them?", and that is the only
    // question worth asking when OoT's logic behaves as if something is missing: an item is either in
    // an OoT location (the fill's own diagnostic lists it if unreachable) or in this list. Anything
    // in neither was lost. Counting made that impossible to check. Skijer's NEI
    std::string announced;
    auto announce = [&](RandomizerGet rg) {
        StartingInventory.push_back(rg);
        announced += announced.empty() ? "" : ", ";
        announced += Rando::StaticData::RetrieveItem(rg).GetName().GetEnglish();
    };
    for (auto& [fcIdx, rcName] : placedMmFcNames) {
        announce((RandomizerGet)sComboFc[fcIdx].rg);
    }
    // The restricted stages' MM placements, but ONLY the ones MM actually reached during the race.
    // This used to hand over all of them unconditionally, which is a lie of exactly the kind the
    // announcement model exists to prevent: an item sitting in a Termina check nobody can get to is
    // not obtainable, and telling OoT's logic otherwise produces a seed that validates and cannot be
    // played. MM reports what it stood on (prePlacedReached), so the honest set is known.
    //
    // An unreached one is not something to work around — it means this arrangement is unplayable, so
    // the attempt is retried. Naming it matters: the whole reason a stranded dungeon reward was so
    // hard to find is that nothing ever said "this item exists but nobody can pick it up".
    //
    // FINAL SETTLEMENT FIRST. The per-turn reports only cover spots MM happened to stand on while it
    // still had items to place; a spot it could reach perfectly well but only AFTER its last turn was
    // never mentioned. Measured: MM's own crawl answered "14 of 14 reachable" while the host called
    // all 14 stranded and retried 30 times. So ask once more here, with everything now placed and
    // announced, which is the only inventory that answers the real question — can MM stand on this
    // spot in the finished seed? Skijer's NEI
    {
        std::vector<std::pair<int, int>> fcItems;
        for (size_t i = 0; i < sComboFc.size(); i++) {
            if (sComboFc[i].count > 0) {
                fcItems.push_back({ sComboFc[i].fcId, sComboFc[i].count });
            }
        }
        std::map<std::string, int> mmAgg;
        for (auto& name : sMmPoolProg) {
            if (!riToFc.contains(name)) {
                mmAgg[name]++;
            }
        }
        std::vector<std::pair<std::string, int>> mmItems(mmAgg.begin(), mmAgg.end());
        nlohmann::json resp = OracleBlocking(FleetOracle_SendReachableRequest(fcItems, mmItems), "reachable", 45000);
        if (resp.contains("prePlacedReached") && resp["prePlacedReached"].is_array()) {
            for (auto& checkJson : resp["prePlacedReached"]) {
                stageAnnouncedMm.insert(checkJson.get<std::string>());
            }
        }
    }

    std::string stranded;
    for (auto& [check, rg] : sStagePlacedMm) {
        if (stageAnnouncedMm.contains(check)) {
            announce((RandomizerGet)rg);
            continue;
        }
        stranded += stranded.empty() ? "" : ", ";
        stranded += Rando::StaticData::RetrieveItem((RandomizerGet)rg).GetName().GetEnglish();
        stranded += " @ ";
        stranded += check;
    }
    if (!stranded.empty()) {
        SPDLOG_ERROR("[FleetComboRando] STRANDED IN MM: MM never reached these pre-placed spots, so OoT "
                     "cannot obtain them - retrying: {}",
                     stranded);
        SetStatus("Delegated fill: an item is stranded in Termina - retrying");
        return false;
    }
    if (!announced.empty()) {
        SPDLOG_INFO("[FleetComboRando] {} items are in MM; announced to OoT's logic as obtainable:\n    {}",
                    placedMmFcNames.size() + sStagePlacedMm.size(), announced);
    }

    // ---- 7) MM junk: every manifest check the turn loop left empty gets local filler ----
    std::unordered_set<std::string> mmTaken(mmUsedNames.begin(), mmUsedNames.end());
    std::vector<std::string> junk = sMmPoolJunk;
    std::shuffle(junk.begin(), junk.end(), sRng);
    size_t junkIdx = 0;
    for (auto& check : sMmChecks) {
        if (mmTaken.contains(check.name)) {
            continue;
        }
        sMmPlacements[check.name] = junkIdx < junk.size() ? junk[junkIdx++] : "RI_JUNK";
    }

    SetStatus("Delegated fill OK: " + std::to_string(placedCount) + "/" + std::to_string(totalToPlace) +
              " items placed (" + std::to_string(mmUsedNames.size()) + " in MM, " +
              std::to_string(placedMmFcNames.size()) + " of them shared)");
    return true;
}

// ---------- spoiler MM + prepareSeed (Fase 3) ----------

std::filesystem::path SelfExeDir() {
#ifdef _WIN32
    wchar_t buf[MAX_PATH];
    DWORD len = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    if (len == 0 || len == MAX_PATH) {
        return {};
    }
    return std::filesystem::canonical(buf).parent_path();
#else
    return std::filesystem::canonical("/proc/self/exe").parent_path();
#endif
}

nlohmann::json sLastMmSpoiler; // el spoiler MM de la última generación (para el .fleet)

// OoT area where each cross-game FC item that did NOT land in MM ended up (RI_name -> "Kakariko
// Village"). It travels in the spoiler so MM's hints (remains, transformations...) can name the real
// place instead of falling back to "in an Unknown Location". Mirror image of the manifest's
// `checkAreas`, which solves the opposite direction. Skijer's NEI
nlohmann::json BuildOotItemAreas() {
    nlohmann::json out = nlohmann::json::object();
    auto ctx = Rando::Context::GetInstance();

    // What already sits in MM is resolved by MM itself; here we only care about the OoT side.
    std::unordered_set<std::string> placedInMm;
    for (auto& [rcName, riName] : sMmPlacements) {
        placedInMm.insert(riName);
    }

    for (auto& fc : sComboFc) {
        if (fc.riName.empty() || fc.riName == "FCI_NO_ITEM" || placedInMm.contains(fc.riName)) {
            continue;
        }
        for (RandomizerCheck loc : ctx->allLocations) {
            if ((int)ctx->GetItemLocation(loc)->GetPlacedRandomizerGet() != fc.rg) {
                continue;
            }
            if (ctx->GetItemLocation(loc)->GetAreas().empty()) {
                break; // no area assigned: better to leave it out than to send garbage
            }
            RandomizerArea area = ctx->GetItemLocation(loc)->GetRandomArea();
            out[fc.riName] =
                Rando::StaticData::hintTextTable[Rando::StaticData::areaNames[area]].GetClear().GetForCurrentLanguage(
                    MF_CLEAN);
            break;
        }
    }
    SPDLOG_INFO("[FleetComboRando] MM spoiler: {} OoT areas for cross-game hints", out.size());
    return out;
}

void WriteMmSpoilerAndPrepare(const std::string& seedString) {
    auto ctx = Rando::Context::GetInstance();

    nlohmann::json spoiler;
    spoiler["type"] = "2S2H_RANDO_SPOILER";
    spoiler["inputSeed"] = seedString;
    spoiler["finalSeed"] = (uint32_t)ctx->GetSeed();
    // Publish the seed identity so MM can VALIDATE its paired save against it. Without this an MM
    // file left over from an older seed loads happily next to a new OoT seed, and the desync only
    // shows up hours later as checks handing out the wrong items.
    FleetShipCombo_SetComboSeed((uint32_t)ctx->GetSeed());
    spoiler["options"] = sMmOptions;
    spoiler["startingItems"] = sMmStarting;
    nlohmann::json checks = nlohmann::json::object();
    for (auto& [checkName, itemName] : sMmPlacements) {
        // 5.0.0 shop/Tingle checks carry a price; 2ship's Spoiler::Apply reads it only from the
        // OBJECT form ({randoItemId, price}); a bare string applies at price 0.
        auto priceIt = sMmCheckPrices.find(checkName);
        if (priceIt != sMmCheckPrices.end()) {
            checks[checkName] = { { "randoItemId", itemName }, { "price", priceIt->second } };
        } else {
            checks[checkName] = itemName;
        }
    }
    // Excluded checks (5.0.0): NOT in the fill (never in the manifest's check list), but GeneratePools
    // moved their vanilla item into the pool and marked them skipped junk. Write them so exactly, or
    // the apply treats a missing check as vanilla and the item exists twice.
    for (auto& checkName : sMmSkippedChecks) {
        if (!checks.contains(checkName)) {
            checks[checkName] = { { "randoItemId", "RI_JUNK" }, { "skipped", true } };
        }
    }
    spoiler["checks"] = checks;
    spoiler["ootItemAreas"] = BuildOotItemAreas();
    // 2ship 5.0.0 reads spoiler["sariaPriorityItems"] unguarded when applying a spoiler; a missing key
    // there aborts the apply and leaves MM's file vanilla/invalid. The combo doesn't use it: empty.
    spoiler["sariaPriorityItems"] = nlohmann::json::array();
    sLastMmSpoiler = spoiler;
    // metadata combo (2ship la ignora al aplicar; la Fase 5 la leerá para las metas)
    spoiler["fleetCombo"] = { { "goalMode", CVarGetInteger("gFleetCombo.GoalMode", 0) },
                              { "triforceTotal", CVarGetInteger("gFleetCombo.TriforceTotal", 15) },
                              { "triforceRequired", CVarGetInteger("gFleetCombo.TriforceRequired", 10) } };

    std::error_code ec;
    std::filesystem::create_directories(SelfExeDir() / "fleet", ec);
    std::filesystem::path bridge = SelfExeDir() / "fleet" / "oracle_spoiler.json";
    {
        std::filesystem::path tmp = bridge;
        tmp += ".tmp";
        std::ofstream out(tmp);
        out << spoiler << std::endl;
        out.close();
        std::filesystem::rename(tmp, bridge);
    }

    std::string fileName = "combo_" + seedString + ".json";
    nlohmann::json resp = OracleBlocking(FleetOracle_SendPrepareSeedRequest(fileName), "prepareSeed");
    SPDLOG_INFO("[FleetComboRando] MM spoiler installed: {} (index {})", fileName, resp.value("spoilerIndex", 0));
}

// Spoiler de VERIFICACIÓN: dónde quedó cada item en ambos mundos + conteos por item, para
// comprobar cantidades/nombres a mano. Se escribe en <ShipDir>/combo_<seed>_summary.json.
nlohmann::json VerifyComboPlaythrough(); // defined below; walks both worlds to prove the seed is beatable

void WriteComboSummary(const std::string& seedString) {
    auto ctx = Rando::Context::GetInstance();
    nlohmann::json summary;
    summary["seed"] = seedString;

    // Placements + conteos de OoT (nombres display del itemTable de soh)
    nlohmann::json ootPlacements = nlohmann::json::object();
    std::map<std::string, int> ootCounts;
    for (RandomizerCheck rc : ctx->allLocations) {
        auto* loc = ctx->GetItemLocation(rc);
        if (loc == nullptr || loc->GetPlacedRandomizerGet() == RG_NONE) {
            continue;
        }
        std::string itemName = Rando::StaticData::RetrieveItem(loc->GetPlacedRandomizerGet()).GetName().GetEnglish();
        auto* staticLoc = Rando::StaticData::GetLocation(rc);
        std::string locName = staticLoc != nullptr ? staticLoc->GetName() : ("RC_" + std::to_string((int)rc));
        ootPlacements[locName] = itemName;
        ootCounts[itemName]++;
    }
    summary["ootPlacements"] = ootPlacements;
    summary["ootItemCounts"] = ootCounts;

    // Placements + conteos de MM (nombres RI_* del spoiler)
    nlohmann::json mmPlacements = nlohmann::json::object();
    std::map<std::string, int> mmCounts;
    for (auto& [checkName, itemName] : sMmPlacements) {
        mmPlacements[checkName] = itemName;
        mmCounts[itemName]++;
    }
    summary["mmPlacements"] = mmPlacements;
    summary["mmItemCounts"] = mmCounts;

    // Verificación por item FC cross: cuántas copias cayeron en cada mundo vs las esperadas
    nlohmann::json fcItems = nlohmann::json::array();
    for (auto& fc : sComboFc) {
        std::string ootName = Rando::StaticData::RetrieveItem((RandomizerGet)fc.rg).GetName().GetEnglish();
        int inOot = ootCounts.count(ootName) ? ootCounts[ootName] : 0;
        int inMm = mmCounts.count(fc.riName) ? mmCounts[fc.riName] : 0;
        // VANILLA COPY IN THE NON-SHUFFLING WORLD. A world that does not shuffle an item still has it
        // sitting in its vanilla spot, and that copy shows up in this count without ever having been
        // cross-placed. With OoT's song shuffle off, every shared song read as "expected 1, got 2
        // (1 OoT + 1 MM)" — the MM one was the shared copy, the OoT one was simply where the song
        // always is. Forcing the option away is off the table (each game decides), so the allowance is
        // recorded, not flagged: `expected` grows by the one vanilla copy the ineligible side holds.
        // Whether that vanilla copy actually exists depends on the item having a vanilla location in
        // that world at all (an MM-only song has none in OoT), so the allowance is a TOLERANCE of one
        // extra copy rather than a computed expectation — no guessing either way.
        int allowance = (fc.inOot && fc.inMm) ? 0 : 1;
        int total = inOot + inMm;
        bool ok = total >= fc.count && total <= fc.count + allowance;
        fcItems.push_back({ { "combo", ootName },
                            { "ootName", ootName },
                            { "mmName", fc.riName },
                            { "expected", fc.count },
                            { "eligibleOot", fc.inOot },
                            { "eligibleMm", fc.inMm },
                            { "vanillaCopyAllowed", allowance },
                            { "placedOot", inOot },
                            { "placedMm", inMm },
                            { "total", total },
                            { "ok", ok } });
    }
    summary["fcCrossItems"] = fcItems;

    // ---- POST-GENERATION VALIDATION ----
    // Each game builds its own pool; the combo only decides which world each copy lands in. So the
    // seed is only sane if, per shared item, placedOot + placedMm == the count the pools agreed on.
    // Anything off means a copy was supplied twice or dropped entirely, and it is named here rather
    // than silently shipped inside a seed nobody can finish. Skijer's NEI
    nlohmann::json poolBad = nlohmann::json::object();
    for (auto& e : fcItems) {
        if (!e["ok"].get<bool>()) {
            poolBad[e["ootName"].get<std::string>()] =
                "expected " + std::to_string(e["expected"].get<int>()) + " (+" +
                std::to_string(e["vanillaCopyAllowed"].get<int>()) + " vanilla allowed), got " +
                std::to_string(e["total"].get<int>()) + " (" + std::to_string(e["placedOot"].get<int>()) + " OoT + " +
                std::to_string(e["placedMm"].get<int>()) + " MM)";
        }
    }
    summary["poolValidation"] = { { "ok", poolBad.empty() }, { "mismatches", poolBad } };

    // OFF BY DEFAULT, and that is a retreat, not a preference.
    //
    // VerifyComboPlaythrough walks OoT's region graph by calling ReachabilitySearch AFTER generation
    // has finished. The fill only ever walks that graph while it owns it, and once it is done the
    // graph keeps loose ends — entrances whose parent or destination is RR_NONE. Three separate null
    // dereferences were patched inside UpdateToDAccess and the crash simply moved to the next line of
    // the same function, which is the shape of a wrong approach rather than a missing guard: a
    // diagnostic was being propped up with defensive code in core logic, and it was crashing
    // generation, which is the one thing that must not break.
    //
    // It stays available because it is genuinely useful — it is what proved MM's half completes and
    // caught the stranded-item class of bug — but it has to be asked for. Turn it on with
    // gFleetCombo.VerifyPlaythrough when a seed needs investigating. Doing it properly means running
    // the walk INSIDE the fill, where the graph is still whole. Skijer's NEI
    if (!CVarGetInteger("gFleetCombo.VerifyPlaythrough", 0)) {
        summary["comboPlaythrough"] = { { "beatable", nullptr },
                                        { "skipped", "set gFleetCombo.VerifyPlaythrough to run it" } };
    } else {
        try {
            summary["comboPlaythrough"] = VerifyComboPlaythrough();
        } catch (const std::exception& e) {
            SPDLOG_ERROR("[FleetComboRando] cross-game playthrough could not run: {}", e.what());
            summary["comboPlaythrough"] = { { "beatable", nullptr }, { "error", e.what() } };
        } catch (...) {
            SPDLOG_ERROR("[FleetComboRando] cross-game playthrough could not run (unknown error)");
            summary["comboPlaythrough"] = { { "beatable", nullptr }, { "error", "unknown" } };
        }
    }
    if (poolBad.empty()) {
        SPDLOG_INFO("[FleetComboRando] pool validation OK: every shared item matches its expected count");
    } else {
        SPDLOG_ERROR("[FleetComboRando] pool validation FAILED for {} shared items - see the summary", poolBad.size());
        for (auto& [name, why] : poolBad.items()) {
            SPDLOG_ERROR("[FleetComboRando]   {}: {}", name, why.get<std::string>());
        }
    }

    // ---- UNSHARED REPORT: what each game has that the combo does NOT know about ----
    //
    // Every item a game shuffles but that has no entry in the FC table stays locked to its own world.
    // That is fine for game-specific things, but it is ALSO how a new upstream feature silently fails
    // to cross: when MM or OoT adds a shuffle we never wired, the item just never appears in the
    // shared pool and nobody notices for months. (Progressive Strength sat with an FCI_NO_ITEM peer
    // exactly like this.) Emitting the list every generation makes that visible on day one.
    //
    // `reason` distinguishes the two cases that look identical from the outside:
    //   "no FC entry"       -> the item is not in FleetComboItems.h at all; add an X() row to share it
    //   "FC peer missing"   -> there IS a row, but the other game's side is empty/FCI_NO_ITEM
    // Skijer's NEI
    std::unordered_set<int> fcRgsAll;
    std::unordered_set<std::string> fcRiAll;
    std::unordered_set<std::string> fcRgHalfPaired; // OoT side present, MM peer empty
    for (int i = 0; i < FC_COMBO_ITEM_COUNT; i++) {
        int rg = FcCombo_NativeForItem(gFcComboItems[i].fcId);
        std::string peer = FcCombo_PeerNameForItem(gFcComboItems[i].fcId);
        if (rg != FCI_NO_ITEM) {
            fcRgsAll.insert(rg); // also keeps NOT_SHARED rows out of the "no FC entry" list
            // A NOT_SHARED row has an empty peer BY DESIGN, so it is not a missing pairing.
            if ((peer == "FCI_NO_ITEM" || peer.empty()) && !(gFcComboItems[i].flags & FCI_F_NOT_SHARED)) {
                fcRgHalfPaired.insert(gFcComboItems[i].comboName);
            }
        }
        if (peer != "FCI_NO_ITEM" && !peer.empty()) {
            fcRiAll.insert(peer);
        }
    }

    nlohmann::json unsharedOot = nlohmann::json::object();
    nlohmann::json shopLocalOot = nlohmann::json::object();
    for (RandomizerCheck rc : ctx->allLocations) {
        auto* loc = ctx->GetItemLocation(rc);
        if (loc == nullptr || loc->GetPlacedRandomizerGet() == RG_NONE) {
            continue;
        }
        RandomizerGet rg = loc->GetPlacedRandomizerGet();
        if (fcRgsAll.contains((int)rg)) {
            continue;
        }
        auto& item = Rando::StaticData::RetrieveItem(rg);
        if (!item.IsAdvancement()) {
            continue; // junk/rupees would drown the report; only progression matters here
        }
        // Shop slots are an OoT-only mechanic (there is no shop check to cross into), so every "Buy X"
        // is expected to stay local. Left in the main list they were 18 of 32 entries and buried the
        // six that are actual wiring gaps. Reported apart, not hidden. Skijer's NEI
        if (item.GetItemType() == ITEMTYPE_SHOP) {
            shopLocalOot[item.GetName().GetEnglish()] = "shop slot, OoT-only by design";
            continue;
        }
        // Known OoT-only by nature, verified case by case — listing them every seed only buried the
        // entries that DO need wiring. Giant's Knife is the pre-Biggoron step (MM has no equivalent),
        // the WINNER heart is the lottery variant, and the Triforce is the goal object itself (the
        // piece that crosses is RG_TRIFORCE_PIECE). Skijer's NEI
        if (rg == RG_GIANTS_KNIFE || rg == RG_TREASURE_GAME_HEART || rg == RG_TRIFORCE) {
            shopLocalOot[item.GetName().GetEnglish()] = "OoT-only by nature";
            continue;
        }
        unsharedOot[item.GetName().GetEnglish()] = "no FC entry";
    }

    nlohmann::json unsharedMm = nlohmann::json::object();
    for (auto& name : sMmPoolProg) { // MM's own progression pool, post FC dedupe
        if (!fcRiAll.contains(name)) {
            unsharedMm[name] = "no FC entry";
        }
    }

    nlohmann::json halfPaired = nlohmann::json::array();
    for (auto& n : fcRgHalfPaired) {
        halfPaired.push_back(n);
    }

    nlohmann::json localOnlyMm = nlohmann::json::array();
    for (auto& s : sLocalOnlyMm) {
        localOnlyMm.push_back(s);
    }

    summary["unshared"] = { { "oot", unsharedOot },
                            { "mm", unsharedMm },
                            { "fcRowsMissingMmPeer", halfPaired },
                            { "placedLocallyMm", localOnlyMm },
                            { "ootShopLocal", shopLocalOot } };
    SPDLOG_INFO("[FleetComboRando] unshared report: {} OoT progression items, {} MM items, {} FC rows "
                "with no MM peer, {} MM placements kept local",
                unsharedOot.size(), unsharedMm.size(), halfPaired.size(), localOnlyMm.size());

    std::error_code ec;
    std::filesystem::create_directories(SelfExeDir() / "fleet", ec);
    std::filesystem::path out = SelfExeDir() / "fleet" / ("combo_" + seedString + "_summary.json");
    std::ofstream file(out);
    file << summary.dump(4) << std::endl;
    SPDLOG_INFO("[FleetComboRando] summary de verificación: {}", out.string());
}

// ---------- .fleet: guardar/cargar una seed combo completa (ambos spoilers) ----------

std::filesystem::path FleetDir() {
    std::error_code ec;
    std::filesystem::create_directories(SelfExeDir() / "fleet", ec);
    return SelfExeDir() / "fleet";
}

// Escribe <ShipDir>/fleet/<seed>.fleet = { oot spoiler, mm spoiler, combo config }.
// El spoiler de OoT se toma del archivo que SpoilerLog_Write dejó (ruta en gGeneral.SpoilerLog).
void WriteFleetFile(const std::string& seedString) {
    nlohmann::json fleet;
    fleet["type"] = "FLEET_COMBO_SEED";
    fleet["version"] = 1;
    fleet["seed"] = seedString;
    fleet["combo"] = { { "goalMode", CVarGetInteger("gFleetCombo.GoalMode", 0) },
                       { "triforceTotal", CVarGetInteger("gFleetCombo.TriforceTotal", 15) },
                       { "triforceRequired", CVarGetInteger("gFleetCombo.TriforceRequired", 10) } };
    fleet["mm"] = sLastMmSpoiler;

    // OoT spoiler: SpoilerLog_Write (dentro de GenerateRandomizer) dejó la ruta en este CVar.
    std::string ootPath = CVarGetString("gGeneral.SpoilerLog", "");
    if (!ootPath.empty()) {
        try {
            std::ifstream in(ootPath);
            nlohmann::json ootSpoiler;
            in >> ootSpoiler;
            fleet["oot"] = ootSpoiler;
        } catch (...) { SPDLOG_WARN("[FleetComboRando] no se pudo leer el spoiler OoT en {}", ootPath); }
    }

    std::filesystem::path out = FleetDir() / (seedString + ".fleet");
    std::ofstream file(out);
    file << fleet.dump(2) << std::endl;
    SPDLOG_INFO("[FleetComboRando] .fleet guardado: {}", out.string());
}

std::vector<std::string> ListFleetFiles() {
    std::vector<std::string> out;
    std::error_code ec;
    for (auto& e : std::filesystem::directory_iterator(FleetDir(), ec)) {
        if (e.is_regular_file() && e.path().extension() == ".fleet") {
            out.push_back(e.path().filename().string());
        }
    }
    std::sort(out.begin(), out.end());
    return out;
}

// Carga una seed combo desde un .fleet SIN regenerar: aplica el spoiler OoT al Context
// (ParseSpoiler -> mSpoilerLoaded) y manda el spoiler MM a 2ship (prepareSeed).
void LoadFleetThread(std::string fileName, int slot, std::string name) {
    // Same reason as GenerateCombo: a half-applied load must not leave the previous "ready" flags up.
    Rando::Context::GetInstance()->SetSeedGenerated(false);
    Rando::Context::GetInstance()->SetSpoilerLoaded(false);
    try {
        SetStatus("Loading " + fileName + "...");
        std::filesystem::path path = FleetDir() / fileName;
        nlohmann::json fleet;
        {
            std::ifstream in(path);
            in >> fleet;
        }
        if (!fleet.contains("type") || fleet["type"] != "FLEET_COMBO_SEED") {
            throw std::runtime_error("not a FLEET_COMBO_SEED file");
        }

        // Config combo
        if (fleet.contains("combo")) {
            auto& c = fleet["combo"];
            CVarSetInteger("gFleetCombo.GoalMode", c.value("goalMode", 0));
            CVarSetInteger("gFleetCombo.TriforceTotal", c.value("triforceTotal", 15));
            CVarSetInteger("gFleetCombo.TriforceRequired", c.value("triforceRequired", 10));
        }

        // OoT: escribir el spoiler embebido a Randomizer/ y ParseSpoiler (fija mSpoilerLoaded=true)
        if (fleet.contains("oot")) {
            std::string ootFile = Ship::Context::GetPathRelativeToAppDirectory("Randomizer/_fleet_oot.json");
            std::error_code ec;
            std::filesystem::create_directories(std::filesystem::path(ootFile).parent_path(), ec);
            {
                std::ofstream out(ootFile);
                out << fleet["oot"] << std::endl;
            }
            Rando::Context::GetInstance()->ParseSpoiler(ootFile.c_str());
            CVarSetString("gGeneral.SpoilerLog", ("./Randomizer/_fleet_oot.json"));
        } else {
            throw std::runtime_error(".fleet has no OoT spoiler");
        }

        // Publish the loaded seed's identity so MM validates its paired save against THIS seed and
        // not whatever it happened to have (same reason as in WriteMmSpoilerAndPrepare).
        if (fleet.contains("mm") && fleet["mm"].contains("finalSeed")) {
            FleetShipCombo_SetComboSeed(fleet["mm"]["finalSeed"].get<uint32_t>());
        }

        // MM: mandar el spoiler embebido al bridge + prepareSeed en 2ship
        if (fleet.contains("mm")) {
            std::error_code ec;
            std::filesystem::create_directories(SelfExeDir() / "fleet", ec);
            std::filesystem::path bridge = SelfExeDir() / "fleet" / "oracle_spoiler.json";
            {
                nlohmann::json mm = fleet["mm"];
                if (!mm.contains("sariaPriorityItems")) {
                    mm["sariaPriorityItems"] = nlohmann::json::array(); // older .fleet: see WriteMmSpoilerAndPrepare
                }
                std::filesystem::path tmp = bridge;
                tmp += ".tmp";
                std::ofstream out(tmp);
                out << mm << std::endl;
                out.close();
                std::filesystem::rename(tmp, bridge);
            }
            std::string mmFile = "combo_" + fleet.value("seed", std::string("fleet")) + ".json";
            OracleBlocking(FleetOracle_SendPrepareSeedRequest(mmFile), "prepareSeed");
        } else {
            throw std::runtime_error(".fleet has no MM spoiler");
        }

        Rando::Context::GetInstance()->SetSpoilerLoaded(true);

        // Loading a .fleet now BAKES the seed straight into the chosen slot in BOTH games (so OoT
        // File N and MM File N always share this seed — no separate "Create save pair" step, which was
        // the source of "MM loaded a different seed"). MM's slot file is written now via the oracle;
        // OoT's is created the next time you reach its title/file-select (FleetCreateSaveTick).
        if (slot >= 0 && slot <= 2) {
            // Encode the name to the file-select charset (digits, A-Z=10.., a-z=36.., space/pad=62).
            unsigned char encoded[8];
            for (int i = 0; i < 8; i++) {
                encoded[i] = 62;
                char c = i < (int)name.size() ? name[i] : '\0';
                if (c >= '0' && c <= '9') {
                    encoded[i] = (unsigned char)(c - '0');
                } else if (c >= 'A' && c <= 'Z') {
                    encoded[i] = (unsigned char)(10 + c - 'A');
                } else if (c >= 'a' && c <= 'z') {
                    encoded[i] = (unsigned char)(36 + c - 'a');
                }
            }
            FleetCombo_RequestCreateSave(slot, encoded); // OoT (deferred to file-select)
            OracleBlocking(FleetOracle_SendCreateSaveRequest(slot, name), "createSave"); // MM (written now)
            SetStatus("Loaded " + fileName + " into File " + std::to_string(slot + 1) +
                      ". MM slot written; go to OoT's title/file-select to finish OoT's slot, then load File " +
                      std::to_string(slot + 1) + ".");
        } else {
            SetStatus("Loaded " + fileName + ". Use 'Create saves in BOTH games' below, then load File N.");
        }
    } catch (const std::exception& e) { SetStatus(std::string("Load failed: ") + e.what()); } catch (...) {
        // Anything that is not a std::exception would otherwise escape this thread and call
        // std::terminate. That kills the process mid-shutdown, and SoH writes its CVars on exit, so
        // the user loses their whole shipofharkinian.json. A failed load must never cost settings.
        SetStatus("Load failed: unknown error");
    }
    sRunning = false;
}

// ---------- thread principal ----------

// Aplica los settings COMBO unificados a AMBOS generadores antes de generar (overwrite), para que
// las dos lógicas se generen con la misma config. gFleetCombo.ItemPool: 0 Scarce/1 Normal/2 Plentiful;
// gFleetCombo.Logic: 0 Glitchless/1 No Logic; gFleetCombo.StartingAge: 0 Child/1 Adult/2 Random.
void ApplyComboSettingsToBothGames() {
    int itemPool = CVarGetInteger("gFleetCombo.ItemPool", 1);
    int logic = CVarGetInteger("gFleetCombo.Logic", 0);
    int extEquipment =
        CVarGetInteger("gCheats.ExtEquip.Enabled", 0) || CVarGetInteger("gRandoSettings.ExtEquipment", 0);

    // --- OoT (gRandoSettings.*; SetAllToContext los leerá luego) ---
    // Item pool 3-way -> OoT 4-value: Scarce=2, Normal=Balanced=1, Plentiful=0.
    CVarSetInteger("gRandoSettings.ItemPool", itemPool == 0 ? 2 : (itemPool == 2 ? 0 : 1));
    CVarSetInteger("gRandoSettings.LogicRules", logic);
    // FleetCombo treats the runtime equipment switch and its OoT pool option as
    // one feature. Otherwise the shared switch works in-game but contributes no
    // equipment rows to the pool that is linked to the MM Oracle.
    CVarSetInteger("gCheats.ExtEquip.Enabled", extEquipment);
    CVarSetInteger("gRandoSettings.ExtEquipment", extEquipment);
    CVarSave();

    // NOTHING is forced here. Under the delegated fill each game owns its own options, so overriding
    // the player's settings behind their back is exactly the wrong behaviour — and it was silently
    // unticking menu boxes (Overworld Spawns) on every generation. The old incompatibility table and
    // its Compatibility panel are gone too: with nothing being forced, every row had degraded to
    // advice, and two of them described changes no code performed. Skijer's NEI

    // --- MM (gRando.Options.*; el manifest los leerá) — push bloqueante ANTES del manifest ---
    // Solo lo COMBO-global (pool/logic/triforce). El resto de opciones rando es POR-JUEGO
    // (decisión: se editan en el menú de cada juego; el combo no las sobreescribe).
    std::vector<std::pair<std::string, int>> mm = {
        { "gRando.Options.RO_PLENTIFUL_ITEMS", itemPool == 2 ? 1 : 0 }, // MM: solo plentiful on/off
        { "gRando.Options.RO_LOGIC", logic },
        { "gCheats.ExtEquip.Enabled", extEquipment },
    };
    // Triforce Hunt combo -> MM pieces (el goal combo lo maneja; sincroniza el shuffle flag + counts)
    mm.push_back(
        { "gRando.Options.RO_SHUFFLE_TRIFORCE_PIECES", CVarGetInteger("gFleetCombo.GoalMode", 0) == 1 ? 1 : 0 });
    mm.push_back({ "gRando.Options.RO_TRIFORCE_PIECES_MAX", CVarGetInteger("gFleetCombo.TriforceTotal", 15) });
    mm.push_back({ "gRando.Options.RO_TRIFORCE_PIECES_REQUIRED", CVarGetInteger("gFleetCombo.TriforceRequired", 10) });
    OracleBlocking(FleetOracle_SendSetOptionsRequest(mm), "applyMmSettings");
}

void GenerateCombo(std::string seedString) {
    // The file-select "Start Combo" gate is IsSeedGenerated()||IsSpoilerLoaded(). Clear BOTH now: a
    // previous successful generation/load left them true, and the native fill below refills the
    // Rando context IN PLACE, so if any MM step after it fails (manifest, prepareSeed, .fleet) the
    // context holds a NEW OoT fill while the flags still say "ready" -> Start bakes an OoT file whose
    // seed MM never received (MM then pairs it with the PREVIOUS spoiler = permanent seedMismatch).
    // Only the successful end of this function sets it back. Skijer's NEI
    Rando::Context::GetInstance()->SetSeedGenerated(false);
    Rando::Context::GetInstance()->SetSpoilerLoaded(false);
    try {
        SetStatus("Applying combo settings to both games...");
        ApplyComboSettingsToBothGames();

        SetStatus("Requesting MM manifest...");
        nlohmann::json manifest = OracleBlocking(FleetOracle_SendManifestRequest(), "manifest");
        ParseManifest(manifest);
        PrepareComboFcItems();
        // Before a single attempt runs: a bad name here has to reach the user, not be retried away.
        ValidatePartialPlando();

        SetStatus("Manifest OK: " + std::to_string(sMmChecks.size()) + " MM checks, " +
                  std::to_string(sMmPoolProg.size()) + " MM prog, " + std::to_string(sComboFc.size()) +
                  " cross FC items");

        auto ctx = Rando::Context::GetInstance();
        Rando::Settings::GetInstance()->SetAllToContext();
        // NOTHING IS FORCED ANY MORE.
        //
        // The old code slammed every entrance shuffle to 0 and songs to Anywhere, because the
        // monolithic pre-placement claimed any location it liked and could not coexist with the
        // restricted stages. That is gone: the delegated fill runs AFTER each game's own restricted
        // stages and only takes what they leave, so whatever a game says about its own shuffles just
        // holds. Forcing options is a contradiction of delegating.
        //
        // Side effect worth knowing: this is also what made "Overworld Spawns" untick itself in the
        // menu whenever a combo seed was generated (tester report). Skijer's NEI
        // The wallet scale used to be forced here (no shuffled child wallet, no Tycoon) because the
        // shared state syncs the wallet LEVEL and MM has no equivalent for either end of OoT's scale.
        // That is a representation mismatch, NOT something the combo owns: under delegation each game
        // keeps its own wallet options and the shared level maps as well as it can. Removed.
        //
        // The ONLY thing still set below is the win condition, and that is not an incompatibility —
        // it is the combo's own goal, which is genuinely cross-game.
        // El goal Triforce del combo maneja sus propias piezas (FC, contador comboTriforce):
        // el sistema nativo de Triforce Hunt de SoH queda apagado durante la generación combo.
        // Upstream sustituyó RSK_TRIFORCE_HUNT por un selector de condición de victoria, donde el
        // Triforce Hunt nativo es ahora RO_WINCON_TRIFORCE_PIECES. Fijar DEFEAT_GANON deja la
        // Trifuerza en Ganon (item_pool.cpp) y desactiva toda recolección nativa, que es lo que
        // hacía el Set(0) anterior — DEFEAT_GANON es además el primer valor del enum.
        ctx->GetOption(RSK_WINCON).Set(RO_WINCON_DEFEAT_GANON);

        // ...and empty the piece counter, which is a SEPARATE setting. item_pool.cpp guards the
        // pieces on `RSK_TRIFORCE_HUNT_PIECES_TOTAL > 0`, not on the win condition, so DEFEAT_GANON
        // stopped the collecting while OoT's own total (100 by default in these settings) still
        // poured 100 Triforce Pieces into a Beat Both Bosses seed. They served no purpose there: no
        // goal counts them and nothing in the combo hands them out.
        //
        // Zero in BOTH modes, because the combo owns the Triforce either way — on Triforce Hunt the
        // pieces come from the FC row above, in the shared amount, so a native supply on top would
        // double it. Skijer's NEI
        ctx->GetOption(RSK_TRIFORCE_HUNT_PIECES_TOTAL).Set(0);

        if (seedString.empty()) {
            seedString = std::to_string(std::random_device{}());
        }
        sRng.seed((uint32_t)std::hash<std::string>{}(seedString));

        CVarSetInteger("gGeneral.RandoGenerating", 1);
        sComboActive = true;
        SetStatus("Generating (native fill + combo pre-placement)...");
        bool ok = GenerateRandomizer({}, {}, seedString);
        sComboActive = false;
        CVarSetInteger("gGeneral.RandoGenerating", 0);

        if (!ok) {
            throw std::runtime_error("the fill could not find a valid placement (30 attempts)");
        }

        SetStatus("Writing MM spoiler + prepareSeed...");
        WriteMmSpoilerAndPrepare(seedString);
        WriteComboSummary(seedString);
        WriteFleetFile(seedString); // .fleet portable (both spoilers) para recargar/compartir

        Rando::Context::GetInstance()->SetSeedGenerated(true);
        SetStatus("Combo seed ready (" + seedString +
                  "). Use 'Create saves in BOTH games' below, then load "
                  "File N from OoT's file select.");
    } catch (const std::exception& e) {
        sComboActive = false;
        CVarSetInteger("gGeneral.RandoGenerating", 0);
        SetStatus(std::string("Combo generation FAILED: ") + e.what());
    } catch (...) {
        // Same guard as LoadFleetThread: a non-std::exception escaping this thread terminates the
        // process, and the crash lands while SoH is flushing CVars on exit -> the config file is
        // truncated and every setting is lost. Skijer's NEI
        sComboActive = false;
        CVarSetInteger("gGeneral.RandoGenerating", 0);
        SetStatus("Combo generation FAILED: unknown error");
    }
    sRunning = false;
}

} // namespace

// The restricted stages' MM placements, keyed by check, as the RI names MM understands. Sent with
// every reachability question so MM's crawl grants the item when it reaches the check — the same way
// a plando placement works. Without this MM cannot see a third of the shared progression: the items
// are physically in its checks but its logic has no idea, so it can never reach anything gated behind
// them, and can never tell OoT it got there either. Skijer's NEI
nlohmann::json FleetOracle_PrePlacedForOracle() {
    nlohmann::json out = nlohmann::json::object();
    for (auto& [check, rg] : sStagePlacedMm) {
        for (auto& row : sComboFc) {
            if (row.rg == rg && !row.riName.empty() && row.riName != "FCI_NO_ITEM") {
                out[check] = row.riName;
                break;
            }
        }
    }
    return out;
}

bool FleetCombo_RestrictedStageHook() {
    if (!sComboActive) {
        return true;
    }
    try {
        return RunRestrictedStages();
    } catch (const std::exception& e) {
        SetStatus(std::string("Restricted stages failed: ") + e.what());
        return false;
    }
}

bool FleetCombo_PrePlacementHook() {
    if (!sComboActive) {
        return true;
    }
    try {
        return RunDelegatedFill();
    } catch (const std::exception& e) {
        SetStatus(std::string("Pre-placement failed: ") + e.what());
        return false;
    }
}

// Reopened so VerifyComboPlaythrough gets internal linkage, matching its forward declaration up in
// the first anonymous-namespace block (a TU has ONE anonymous namespace, so these are the same one).
namespace {

// CROSS-GAME PLAYTHROUGH. The real "is this seed beatable" check.
//
// SoH's own playthroughBeatable only proves OoT's half: the items the combo put in Termina are fed to
// it as StartingInventory, so its logic treats them as free. Measured on three seeds — 3-4 spheres,
// 13-17 items, and ZERO MM checks in any of them. Nothing verified that Termina's half is reachable
// at all, so a seed where an OoT item sits behind an MM item that itself sits behind that OoT item
// would generate happily and be impossible.
//
// This walks both worlds together instead: collect everything reachable in whichever world is live,
// pool the items (an item found in one world counts in the other — that is what the portal means),
// open the second world when the portal is standable, repeat until nothing new appears. If the walk
// stops with checks left over, the seed is NOT completable and it says so.
nlohmann::json VerifyComboPlaythrough() {
    auto ctx = Rando::Context::GetInstance();
    nlohmann::json out;

    std::unordered_map<std::string, int> riToFcIdx;
    std::unordered_map<int, int> rgToFcIdx;
    for (size_t i = 0; i < sComboFc.size(); i++) {
        riToFcIdx[sComboFc[i].riName] = (int)i;
        rgToFcIdx[sComboFc[i].rg] = (int)i;
    }

    std::vector<int> fcHave(sComboFc.size(), 0); // shared items collected, by FC index
    std::map<std::string, int> mmNativeHave;     // MM items with no FC row
    std::set<int> ootDone;                       // OoT checks already collected
    std::set<std::string> mmDone;                // MM checks already collected
    // OoT items with NO FC row (small keys, maps, compasses, its own one-off progression...). They
    // have to be remembered separately for exactly the same reason mmNativeHave exists: the OoT
    // inventory is REBUILT from scratch each sphere, so anything not held here is silently dropped
    // and the walk keeps re-deriving reachability without it. That is why OoT stalled at 308 of 534
    // progression items while MM finished 282 of 288 — not a broken seed, a leaky inventory.
    std::unordered_map<int, int> ootNativeHave; // RG -> count
    for (auto& name : sMmStarting) {
        auto it = riToFcIdx.find(name);
        if (it != riToFcIdx.end()) {
            fcHave[it->second]++;
        } else {
            mmNativeHave[name]++;
        }
    }

    std::vector<RandomizerCheck> searchLocations = ctx->allLocations;
    searchLocations.push_back(RC_ALTAR_HINT_CHILD);
    searchLocations.push_back(RC_ALTAR_HINT_ADULT);

    // SNAPSHOT THE PLACEMENTS FIRST. The walk has to empty the locations to make the search see them
    // (see the search below), so it needs its own copy of what was placed where — and it must be
    // taken before any search runs. The .fleet is unaffected either way: it is serialized from the
    // spoiler, not from these locations. Skijer's NEI
    std::unordered_map<int, int> placedAt; // RC -> RG, taken before anything resets
    for (RandomizerCheck rc : ctx->allLocations) {
        auto* loc = ctx->GetItemLocation(rc);
        if (loc != nullptr && loc->GetPlacedRandomizerGet() != RG_NONE) {
            placedAt[(int)rc] = (int)loc->GetPlacedRandomizerGet();
        }
    }

    const bool startInMm = CVarGetInteger("gFleetCombo.StartInMM", 0) != 0;
    bool ootOpen = !startInMm, mmOpen = startInMm;
    int portalSphere = -1;
    nlohmann::json spheres = nlohmann::json::array();

    // The walk came back with ZERO spheres on one seed and a full 7-sphere playthrough on the next,
    // from the same build minutes apart — so the opening state is worth stating rather than inferred.
    // These four numbers separate every way it can start dead: nothing placed to collect, neither
    // world open, or the first search simply finding nothing. Skijer's NEI
    SPDLOG_INFO("[FleetComboRando] playthrough walk starts: ootOpen={} mmOpen={} placed={} mmPlacements={}", ootOpen,
                mmOpen, placedAt.size(), sMmPlacements.size());

    for (int sphere = 1; sphere <= 64; sphere++) {
        int gainedOot = 0, gainedMm = 0;
        // Named progression pickups of this sphere, so the flow can be read (and drawn) afterwards.
        // Junk is only counted: listing 700 rupees would bury the items that actually open the seed.
        nlohmann::json sphereItems = nlohmann::json::array();
        int junkOot = 0, junkMm = 0;

        if (ootOpen) {
            logic->Reset();
            for (size_t i = 0; i < sComboFc.size(); i++) {
                for (int k = 0; k < fcHave[i]; k++) {
                    Rando::StaticData::RetrieveItem((RandomizerGet)sComboFc[i].rg).ApplyEffect();
                }
            }
            for (auto& [rgInt, count] : ootNativeHave) {
                for (int k = 0; k < count; k++) {
                    Rando::StaticData::RetrieveItem((RandomizerGet)rgInt).ApplyEffect();
                }
            }
            // EMPTY THE LOCATIONS FOR THE DURATION OF THE SEARCH. A location only enters
            // accessibleLocations when it is still EMPTY (fill.cpp gates the push on
            // `locItem == RG_NONE || logic->CalculatingAvailableChecks`) — the fill is looking for
            // somewhere to put things, not for what is reachable. Searching a FINISHED seed
            // therefore returns an empty list every time, which is why OoT collected 0 of 2549
            // while MM collected 884 in the same walk.
            //
            // calculatingAvailableChecks = true lifts that gate, but ReachabilitySearch then calls
            // logic->Reset(false) internally and wipes the inventory applied just above, so the walk
            // would start from nothing. Emptying the locations does the same job from the outside;
            // what the walk reads comes from `placedAt` anyway, and everything is put straight back.
            // Skijer's NEI
            for (auto& [rcInt, rgInt] : placedAt) {
                ctx->GetItemLocation((RandomizerCheck)rcInt)->SetPlacedItem(RG_NONE);
            }
            std::vector<RandomizerCheck> reachable = ReachabilitySearch(searchLocations);
            if (sphere == 1) {
                // First search only: with an empty inventory OoT should still open Kokiri Forest and
                // its neighbours. A handful here means the search itself came back dead, which is a
                // different fault from "reached plenty but none of it was collectable".
                SPDLOG_INFO("[FleetComboRando] playthrough sphere 1: OoT search returned {} locations",
                            reachable.size());
            }
            for (auto& [rcInt, rgInt] : placedAt) {
                ctx->GetItemLocation((RandomizerCheck)rcInt)->SetPlacedItem((RandomizerGet)rgInt);
            }
            for (RandomizerCheck rc : reachable) {
                if (!mmOpen && (rc == RC_ALTAR_HINT_CHILD || rc == RC_ALTAR_HINT_ADULT)) {
                    mmOpen = true;
                    portalSphere = sphere;
                }
                if (ootDone.contains((int)rc)) {
                    continue;
                }
                auto placedIt = placedAt.find((int)rc);
                if (placedIt == placedAt.end()) {
                    continue;
                }
                ootDone.insert((int)rc);
                RandomizerGet rg = (RandomizerGet)placedIt->second;
                auto fcIt = rgToFcIdx.find((int)rg);
                if (fcIt != rgToFcIdx.end()) {
                    fcHave[fcIt->second]++; // shared: also counts on MM's side
                } else {
                    ootNativeHave[(int)rg]++; // OoT-only: still has to survive to the next sphere
                }
                auto& item = Rando::StaticData::RetrieveItem(rg);
                if (item.IsAdvancement()) {
                    auto* staticLoc = Rando::StaticData::GetLocation(rc);
                    sphereItems.push_back({ { "world", "OoT" },
                                            { "item", item.GetName().GetEnglish() },
                                            { "check", staticLoc != nullptr ? staticLoc->GetName() : "?" },
                                            { "shared", fcIt != rgToFcIdx.end() } });
                } else {
                    junkOot++;
                }
                gainedOot++;
            }
        }

        if (mmOpen) {
            std::vector<std::pair<int, int>> fcItems;
            for (size_t i = 0; i < sComboFc.size(); i++) {
                if (fcHave[i] > 0) {
                    fcItems.push_back({ sComboFc[i].fcId, fcHave[i] });
                }
            }
            std::vector<std::pair<std::string, int>> mmItems(mmNativeHave.begin(), mmNativeHave.end());
            nlohmann::json resp =
                OracleBlocking(FleetOracle_SendReachableRequest(fcItems, mmItems), "reachable", 45000);
            if (!ootOpen && resp.value("portalReachable", false)) {
                ootOpen = true;
                portalSphere = sphere;
            }
            if (resp.contains("reachable") && resp["reachable"].is_array()) {
                for (auto& idJson : resp["reachable"]) {
                    auto nameIt = sMmCheckName.find(idJson.get<int>());
                    if (nameIt == sMmCheckName.end() || mmDone.contains(nameIt->second)) {
                        continue;
                    }
                    auto placedIt = sMmPlacements.find(nameIt->second);
                    if (placedIt == sMmPlacements.end()) {
                        continue;
                    }
                    mmDone.insert(nameIt->second);
                    auto fcIt = riToFcIdx.find(placedIt->second);
                    if (fcIt != riToFcIdx.end()) {
                        fcHave[fcIt->second]++; // shared: also counts on OoT's side
                    } else {
                        mmNativeHave[placedIt->second]++;
                    }
                    // MM junk is recognised by name (the host has no MM item table).
                    const std::string& ri = placedIt->second;
                    bool junk = ri == "RI_JUNK" || ri == "RI_NONE" || ri.rfind("RI_RUPEE", 0) == 0 ||
                                ri.rfind("RI_RECOVERY_HEART", 0) == 0 || ri.rfind("RI_MAGIC_JAR", 0) == 0 ||
                                ri.rfind("RI_ARROWS", 0) == 0 || ri.rfind("RI_BOMBS", 0) == 0 ||
                                ri.find("_REFILL") != std::string::npos;
                    if (junk) {
                        junkMm++;
                    } else {
                        sphereItems.push_back({ { "world", "MM" },
                                                { "item", ri },
                                                { "check", nameIt->second },
                                                { "shared", fcIt != riToFcIdx.end() } });
                    }
                    gainedMm++;
                }
            }
        }

        if (gainedOot == 0 && gainedMm == 0) {
            break; // fixed point: nothing new is reachable in either world
        }
        spheres.push_back({ { "sphere", sphere },
                            { "oot", gainedOot },
                            { "mm", gainedMm },
                            { "junkOot", junkOot },
                            { "junkMm", junkMm },
                            { "portalOpens", sphere == portalSphere },
                            { "items", sphereItems } });
    }

    // BEATABLE = every PROGRESSION item was collected, not every check. Rupees in pots and junk left
    // in a corner nobody has to visit do not make a seed unbeatable, and plenty of locations are
    // legitimately unreachable (excluded checks, options set to vanilla). Judging on all 2451 OoT
    // locations called every seed unbeatable for no reason.
    // From the SNAPSHOT, not the live locations: by now ReachabilitySearch has reset them.
    size_t ootTotal = 0, ootProgTotal = 0, ootProgGot = 0;
    for (auto& [rcInt, rgInt] : placedAt) {
        ootTotal++;
        if (Rando::StaticData::RetrieveItem((RandomizerGet)rgInt).IsAdvancement()) {
            ootProgTotal++;
            if (ootDone.contains(rcInt)) {
                ootProgGot++;
            }
        }
    }
    std::unordered_set<std::string> mmJunk(sMmPoolJunk.begin(), sMmPoolJunk.end());
    size_t mmProgTotal = 0, mmProgGot = 0;
    for (auto& [checkName, riName] : sMmPlacements) {
        if (mmJunk.contains(riName)) {
            continue;
        }
        mmProgTotal++;
        if (mmDone.contains(checkName)) {
            mmProgGot++;
        }
    }
    // BEATABLE IS ABOUT THE GOAL, NOT ABOUT 100% COLLECTION.
    //
    // The first version judged on "every progression item collected" and called seeds unbeatable for
    // things that do not block anything. Measured case: seed 2431918113 came out 544 of 545, and the
    // single hold-out was `Ganon's Castle Small Key @ Ganon's Castle MQ Light Trial Pot 2` — a trial
    // key you simply never have to pick up. Nothing about that stops you reaching Ganon.
    //
    // So the verdict now follows the same rule SoH's own fill uses: the seed is beatable when the
    // location holding RG_TRIFORCE (Ganon's prize) has been collected. The 100% numbers stay in the
    // summary as diagnostics — they are useful, they are just not the verdict.
    //
    // MM's half is still a PROXY: all of its reachable progression being collected. A real check
    // ("can Majora be reached") has to come from the oracle, which does not report one yet, so
    // `mmGoalIsProxy` marks it rather than quietly passing it off as verified. Skijer's NEI
    bool ootGoal = false;
    for (auto& [rcInt, rgInt] : placedAt) {
        if ((RandomizerGet)rgInt == RG_TRIFORCE && ootDone.contains(rcInt)) {
            ootGoal = true;
            break;
        }
    }
    const bool mmGoal = mmProgGot >= mmProgTotal;
    const bool complete = ootProgGot >= ootProgTotal && mmProgGot >= mmProgTotal;
    const bool beatable = ootGoal && mmGoal;

    // Name what was left behind, capped. Reverse-engineering the one missing item out of a summary
    // that only said "544/545" cost a whole round trip; this makes the next one immediate.
    nlohmann::json missedOot = nlohmann::json::array();
    for (auto& [rcInt, rgInt] : placedAt) {
        if (ootDone.contains(rcInt) || missedOot.size() >= 40) {
            continue;
        }
        auto& item = Rando::StaticData::RetrieveItem((RandomizerGet)rgInt);
        if (!item.IsAdvancement()) {
            continue;
        }
        auto* staticLoc = Rando::StaticData::GetLocation((RandomizerCheck)rcInt);
        missedOot.push_back({ { "check", staticLoc != nullptr ? staticLoc->GetName() : "?" },
                              { "item", item.GetName().GetEnglish() } });
    }
    nlohmann::json missedMm = nlohmann::json::array();
    for (auto& [checkName, riName] : sMmPlacements) {
        if (mmDone.contains(checkName) || mmJunk.contains(riName) || missedMm.size() >= 40) {
            continue;
        }
        missedMm.push_back({ { "check", checkName }, { "item", riName } });
    }

    out = { { "beatable", beatable },
            { "ootGoalReached", ootGoal },
            { "mmGoalReached", mmGoal },
            { "mmGoalIsProxy", true },
            { "allProgressionCollected", complete },
            { "spheres", spheres },
            { "sphereCount", spheres.size() },
            { "portalOpenedAtSphere", portalSphere },
            { "ootProgression", { { "collected", ootProgGot }, { "total", ootProgTotal } } },
            { "mmProgression", { { "collected", mmProgGot }, { "total", mmProgTotal } } },
            { "ootCollected", ootDone.size() },
            { "ootTotal", ootTotal },
            { "mmCollected", mmDone.size() },
            { "mmTotal", sMmPlacements.size() },
            { "uncollectedProgressionOot", missedOot },
            { "uncollectedProgressionMm", missedMm } };
    if (beatable) {
        SPDLOG_INFO("[FleetComboRando] cross-game playthrough BEATABLE: {} spheres, portal at sphere {}, "
                    "progression OoT {}/{}, MM {}/{}",
                    spheres.size(), portalSphere, ootProgGot, ootProgTotal, mmProgGot, mmProgTotal);
    } else {
        SPDLOG_ERROR("[FleetComboRando] cross-game playthrough NOT beatable after {} spheres "
                     "(OoT goal {}, MM goal {}): progression OoT {}/{}, MM {}/{}",
                     spheres.size(), ootGoal, mmGoal, ootProgGot, ootProgTotal, mmProgGot, mmProgTotal);
    }
    return out;
}

} // namespace

// How many bottles MM contributes that OoT cannot draw itself, so OoT's pool can make room for them.
//
// The bottle inventory is SHARED (FleetSync syncs bottleSlots[8]), so the combo total must be 8. MM's
// own bottle checks mostly hand out contents OoT also has (Empty, Milk, Red Potion) and those collapse
// onto the same FC row — one copy serves both. But Gold Dust and Chateau Romani exist ONLY in MM, so
// they are extra on top of whatever OoT generates: measured 10 bottles instead of 8. Counting them
// rather than hardcoding 2 keeps this right if MM ever adds another exclusive content. Skijer's NEI
int FleetCombo_MmOnlyBottleCount() {
    if (!sComboActive) {
        return 0;
    }
    std::unordered_set<int> ootDrawable;
    for (RandomizerGet rg : Rando::StaticData::normalBottles) {
        ootDrawable.insert((int)rg);
    }
    int extra = 0;
    for (auto& [riName, count] : sMmManifestCounts) {
        if (riName.rfind("RI_BOTTLE_", 0) != 0) {
            continue; // RI_OOT_BOTTLE_* are OoT contents mirrored in MM, not MM exclusives
        }
        auto it =
            std::find_if(sComboFc.begin(), sComboFc.end(), [&](const ComboFcItem& fc) { return fc.riName == riName; });
        if (it != sComboFc.end() && !ootDrawable.contains(it->rg)) {
            extra += count;
        }
    }
    return extra;
}

// Is a combo seed being generated right now? For the few native stages whose numbers differ in a
// combo. Solo-OoT generation is never affected.
bool FleetCombo_IsComboGeneration() {
    return sComboActive;
}

// Shared Songs mode, for the native fill stages that have to stand down when the combo owns song
// placement. Hard 0 outside a combo generation so a solo-OoT seed is never affected by the CVar.
int FleetCombo_SharedSongsMode() {
    if (!sComboActive) {
        return FC_SONGS_OWN_GAME_LOGIC;
    }
    return FcCategoryMode(0);
}

// Same, for Dungeon Rewards. True when the combo owns reward placement across both games, which is
// the signal for OoT's own reward stages (RandomizeDungeonRewards and the reward branches of
// RandomizeDungeonItems) to stand down and leave the 9 boss locations empty for the combo stage.
bool FleetCombo_RestrictedDungeonRewards() {
    return sComboActive && FcCategoryMode(1) == FC_RESTRICT_CATEGORY_SPOTS;
}

std::string FleetCombo_GetMmAreaForItem(const std::string& riName) {
    if (riName.empty() || sMmCheckAreas.empty()) {
        return "";
    }
    // sMmPlacements is RC_name -> RI_name; find the check that received this item and translate to an
    // area. If the same RI_ is placed several times in MM we return the first: a hint only needs one
    // valid place to point at. Skijer's NEI
    for (auto& [rcName, placedRi] : sMmPlacements) {
        if (placedRi != riName) {
            continue;
        }
        auto it = sMmCheckAreas.find(rcName);
        if (it != sMmCheckAreas.end() && !it->second.empty()) {
            return it->second;
        }
    }
    return "";
}

// A hint names a CONCRETE item; the combo may supply it as a CHAIN. The Ganondorf hint asks for
// RG_MASTER_SWORD, the FC table has RG_PROGRESSIVE_MASTER_SWORD, the lookup found nothing and the
// hint printed "the sacred blade from an Isolated Place". Light Arrows in the same sentence resolved
// fine, because that one is not progressive — which is exactly why only half the hint looked broken.
//
// Nothing in SoH maps a concrete item back to the progressive that grants it, so the combo carries
// the correspondence. It is data: a hint that names any of these resolves to wherever its chain is.
// Adding one is a row. Skijer's NEI
const struct {
    int concrete;
    int chain;
} kFcChainAliases[] = {
    { RG_MASTER_SWORD, RG_PROGRESSIVE_MASTER_SWORD },
    { RG_GORONS_BRACELET, RG_PROGRESSIVE_STRENGTH },
    { RG_SILVER_GAUNTLETS, RG_PROGRESSIVE_STRENGTH },
    { RG_GOLDEN_GAUNTLETS, RG_PROGRESSIVE_STRENGTH },
    { RG_SILVER_SCALE, RG_PROGRESSIVE_SCALE },
    { RG_GOLDEN_SCALE, RG_PROGRESSIVE_SCALE },
    { RG_HOOKSHOT, RG_PROGRESSIVE_HOOKSHOT },
    { RG_LONGSHOT, RG_PROGRESSIVE_HOOKSHOT },
    { RG_FAIRY_BOW, RG_PROGRESSIVE_BOW },
    { RG_FAIRY_SLINGSHOT, RG_PROGRESSIVE_SLINGSHOT },
    { RG_BOMB_BAG, RG_PROGRESSIVE_BOMB_BAG },
    { RG_FAIRY_OCARINA, RG_PROGRESSIVE_OCARINA },
    { RG_OCARINA_OF_TIME, RG_PROGRESSIVE_OCARINA },
    { RG_ADULT_WALLET, RG_PROGRESSIVE_WALLET },
    { RG_GIANT_WALLET, RG_PROGRESSIVE_WALLET },
    { RG_TYCOON_WALLET, RG_PROGRESSIVE_WALLET },
    { RG_MAGIC_SINGLE, RG_PROGRESSIVE_MAGIC_METER },
    { RG_MAGIC_DOUBLE, RG_PROGRESSIVE_MAGIC_METER },
    // NEI chains: the give choke sees the resolved tier, so every tier folds onto its FC chain row.
    { RG_RAZOR_SWORD, RG_PROGRESSIVE_KOKIRI_SWORD },
    { RG_GILDED_SWORD, RG_PROGRESSIVE_KOKIRI_SWORD },
    { RG_TRUE_MASTER_SWORD, RG_PROGRESSIVE_MASTER_SWORD },
    { RG_GREAT_FAIRY_SWORD, RG_PROGRESSIVE_BGS },
    { RG_IRON_KNUCKLE_AXE, RG_PROGRESSIVE_HAMMER },
    { RG_ULTRASHOT, RG_PROGRESSIVE_HOOKSHOT },
    { RG_QUARTZ_OF_MOTION, RG_STONE_OF_AGONY },
    { RG_ROCS_CAPE, RG_PROGRESSIVE_ROCS },
    { RG_DEKU_STICK_CAPACITY_20, RG_PROGRESSIVE_STICK_UPGRADE },
    { RG_DEKU_STICK_CAPACITY_30, RG_PROGRESSIVE_STICK_UPGRADE },
    { RG_DEKU_NUT_CAPACITY_30, RG_PROGRESSIVE_NUT_UPGRADE },
    { RG_DEKU_NUT_CAPACITY_40, RG_PROGRESSIVE_NUT_UPGRADE },
    // No tunics or boots here: OoT ships them as individual items (RG_GORON_TUNIC, RG_IRON_BOOTS...),
    // not as a chain, so a hint naming one already resolves through the ordinary rg match.
};

int FleetCombo_ChainAliasFor(int randomizerGet) {
    for (auto& alias : kFcChainAliases) {
        if (alias.concrete == randomizerGet) {
            return alias.chain;
        }
    }
    return 0;
}

int FleetCombo_ChainForItem(int randomizerGet) {
    if (sComboFc.empty()) {
        return 0;
    }
    for (auto& fc : sComboFc) {
        if (fc.rg == randomizerGet) {
            return 0; // the combo carries it under its own id; nothing to translate
        }
    }
    for (auto& alias : kFcChainAliases) {
        if (alias.concrete != randomizerGet) {
            continue;
        }
        for (auto& fc : sComboFc) {
            if (fc.rg == alias.chain) {
                return alias.chain;
            }
        }
        break;
    }
    return 0;
}

std::string FleetCombo_GetMmAreaForOotItem(int randomizerGet) {
    if (sMmCheckAreas.empty() || sComboFc.empty()) {
        return "";
    }
    // RandomizerGet (OoT side) -> RI_ name (MM side) via the FC table, and from there to the area.
    for (auto& fc : sComboFc) {
        if (fc.rg == randomizerGet) {
            return FleetCombo_GetMmAreaForItem(fc.riName);
        }
    }
    // Not in the table under its own id: it may be one level of a chain the combo does carry.
    for (auto& alias : kFcChainAliases) {
        if (alias.concrete != randomizerGet) {
            continue;
        }
        for (auto& fc : sComboFc) {
            if (fc.rg == alias.chain) {
                return FleetCombo_GetMmAreaForItem(fc.riName);
            }
        }
        break;
    }
    return "";
}

bool FleetCombo_HasMmHintData() {
    return !sMmCheckAreas.empty() && !sMmPlacements.empty();
}

bool FleetCombo_StartGeneration(const std::string& seedString) {
    if (sRunning.exchange(true)) {
        return false;
    }
    if (FleetShipCombo_GetActiveGame() < 0) {
        sRunning = false;
        SetStatus("No active combo: MM oracle unavailable");
        return false;
    }
    if (sThread.joinable()) {
        sThread.join();
    }
    sThread = std::thread(GenerateCombo, seedString);
    return true;
}

bool FleetCombo_LoadFleet(const std::string& fileName, int slot, const std::string& name) {
    if (sRunning.exchange(true)) {
        return false;
    }
    if (FleetShipCombo_GetActiveGame() < 0) {
        sRunning = false;
        SetStatus("No active combo: MM oracle unavailable");
        return false;
    }
    if (sThread.joinable()) {
        sThread.join();
    }
    sThread = std::thread(LoadFleetThread, fileName, slot, name);
    return true;
}

std::vector<std::string> FleetCombo_ListFleetFiles() {
    return ListFleetFiles();
}

bool FleetCombo_IsRunning() {
    return sRunning;
}

std::string FleetCombo_GetStatus() {
    std::lock_guard<std::mutex> lock(sStatusMx);
    return sStatus;
}

// ---- File-select "COMBO" (QUEST_OOTXMM) C bridges (called from z_file_choose.c / z_sram.c) ----
#include "FleetShipCombo.h"
#include "FleetOracleClient.h"

// Cache of the .fleet files for the file-select "Load Combo Seed" picker (refreshed on menu open,
// so the DL draw doesn't hit the disk every frame).
static std::vector<std::string> sFsFleetCache;

// Queue a prepareSeed carrying the MM spoiler that belongs to the combo file identified by
// (inputSeed, finalSeed), so MM rebuilds/creates its half of the slot on THIS seed. Sources, in
// order: fleet/<inputSeed>.fleet ["mm"] (every generation and every loaded seed writes/has one), else
// the last spoiler generated this session if its finalSeed matches. Nothing found -> nothing queued:
// MM then keeps whatever it has and reports seedMismatch instead of building a wrong-seed file.
static void QueuePrepareSeedForFile(const std::string& inputSeed, uint32_t finalSeed) {
    if (FleetShipCombo_GetActiveGame() < 0) {
        return; // no MM to prepare
    }
    nlohmann::json mm;
    std::string seedString = inputSeed;
    if (!inputSeed.empty()) {
        std::filesystem::path path = FleetDir() / (inputSeed + ".fleet");
        std::error_code ec;
        if (std::filesystem::exists(path, ec)) {
            try {
                nlohmann::json fleet;
                std::ifstream in(path);
                in >> fleet;
                if (fleet.value("type", "") == "FLEET_COMBO_SEED" && fleet.contains("mm") && fleet["mm"].is_object()) {
                    mm = fleet["mm"];
                }
            } catch (...) { SPDLOG_WARN("[FleetComboFS] .fleet ilegible: {}", path.string()); }
        }
    }
    if (mm.is_null() && sLastMmSpoiler.is_object() && finalSeed != 0 &&
        sLastMmSpoiler.value("finalSeed", 0u) == finalSeed) {
        mm = sLastMmSpoiler; // generated this session, .fleet not written (or not found)
        if (seedString.empty()) {
            seedString = sLastMmSpoiler.value("inputSeed", std::string("fleet"));
        }
    }
    if (mm.is_null()) {
        SPDLOG_WARN("[FleetComboFS] no MM spoiler on hand for seed '{}' (final {}): MM keeps its prepared one",
                    inputSeed, finalSeed);
        return;
    }
    if (finalSeed != 0 && mm.value("finalSeed", 0u) != finalSeed) {
        SPDLOG_WARN("[FleetComboFS] .fleet {} carries MM finalSeed {} but this file is {}: not sending it", inputSeed,
                    mm.value("finalSeed", 0u), finalSeed);
        return;
    }
    if (!mm.contains("sariaPriorityItems")) {
        mm["sariaPriorityItems"] = nlohmann::json::array(); // older .fleet (see WriteMmSpoilerAndPrepare)
    }
    if (seedString.empty()) {
        seedString = "fleet";
    }
    FleetOracle_QueuePrepareSeed("combo_" + seedString + ".json", mm);
    SPDLOG_INFO("[FleetComboFS] queued prepareSeed combo_{}.json (final {}) for the picked combo file", seedString,
                finalSeed);
}

extern "C" {

void FleetComboFS_RefreshFleets(void) {
    sFsFleetCache = FleetCombo_ListFleetFiles();
}

int FleetComboFS_FleetCount(void) {
    return (int)sFsFleetCache.size();
}

const char* FleetComboFS_FleetName(int idx) {
    if (idx < 0 || idx >= (int)sFsFleetCache.size()) {
        return "";
    }
    return sFsFleetCache[idx].c_str();
}

void FleetComboFS_LoadSeedIndex(int idx) {
    if (idx < 0 || idx >= (int)sFsFleetCache.size()) {
        return;
    }
    // slot < 0 = SEED-ONLY: apply the OoT spoiler to the Rando context + send MM prepareSeed, but do
    // NOT bake a save slot. This makes the seed "ready" (IsSpoilerLoaded) so "Start Combo" then
    // creates the OoT+MM save pair at the file-select slot using THIS loaded seed.
    FleetCombo_LoadFleet(sFsFleetCache[idx], -1, "LINK");
}

void FleetComboFS_Generate(void) {
    FleetCombo_StartGeneration(""); // async, own thread; marks the Rando context seed-generated on finish
}

void FleetComboFS_OpenSettings(void) {
    // Open the shared combo randomizer settings = the (trimmed) Fleet Shared "Randomizer" header.
    CVarSetString("gSettings.Menu.ActiveHeader", "Randomizer##FleetShared");
    FleetShipCombo_OpenSharedWindow(); // sets gFleetCombo.MenuMode=1 + shows the SohMenu
}

int FleetComboFS_IsBusy(void) {
    return FleetCombo_IsRunning() ? 1 : 0;
}

void FleetComboFS_OnCreateSave(int slot) {
    if (slot < 0 || slot > 2) {
        return;
    }
    // MM: delete + recreate its paired slot on disk WITH this seed (its live state is untouched).
    // Publish the seed identity first — MM validates the rebuilt slot against it and refuses to
    // build it from any other spoiler — then make sure MM has THIS seed's spoiler installed (a Load
    // .fleet / Generate already sent prepareSeed, but a re-queued one is idempotent and covers the
    // case where 2ship was restarted in between), then create. All through the maintenance queue so
    // the three keep their order and never cut in on the generator's channel.
    auto ctx = Rando::Context::GetInstance();
    uint32_t seed = ctx != nullptr ? (uint32_t)ctx->GetSeed() : 0u;
    if (seed != 0) {
        FleetShipCombo_SetComboSeed(seed);
    }
    QueuePrepareSeedForFile(ctx != nullptr ? ctx->GetSeedString() : std::string(), seed);
    FleetOracle_QueueCreateSave(slot, "LINK"); // MM name is cosmetic (its file select is bypassed)

    // Publish the combo slot cross-process so MM auto-loads the SAME slot when it becomes active.
    FleetShipCombo_SetComboSlot(slot);

    // Bake this file's "start in MM vs OoT" choice from the shared toggle, per slot, so a later
    // launch/boot resumes into the right game.
    int startInMm = CVarGetInteger("gFleetCombo.StartInMM", 0) ? 1 : 0;
    CVarSetInteger(("gFleetCombo.StartInMM.Slot" + std::to_string(slot)).c_str(), startInMm);
    CVarSetInteger("gFleetCombo.LastSlot", slot);
    CVarSave();
    SPDLOG_INFO("[FleetComboFS] combo save created for slot {} (startInMM={})", slot, startInMm);
}

void FleetComboFS_OnLoadSave(int slot) {
    if (slot < 0 || slot > 2) {
        return;
    }
    FleetShipCombo_SetComboSlot(slot); // MM auto-loads this slot when it becomes active

    // This file's seed is now the combo's identity: republish it (the save was just loaded, so the
    // Rando context holds THIS file's finalSeed, which need not be the one generated this session)
    // and have MM prove its half of the slot matches. A missing MM file, or one left over from
    // another seed, is rebuilt from this one — otherwise OoT would be playing seed A while MM plays
    // seed B, and nothing says so until a check hands out the wrong item hours later.
    auto ctx = Rando::Context::GetInstance();
    uint32_t fileSeed = ctx != nullptr ? (uint32_t)ctx->GetSeed() : 0u;
    if (fileSeed != 0) {
        FleetShipCombo_SetComboSeed(fileSeed);
    }
    // Hand MM THIS file's spoiler before asking it to check/rebuild its half. Without this, an
    // ensureSave rebuild used "whatever spoiler 2ship prepared last" (= the last seed generated or
    // loaded), so picking an older combo file rebuilt MM's slot on the wrong fill and the pair
    // never matched ("MM no pudo parear el slot"). The spoiler comes from fleet/<inputSeed>.fleet.
    QueuePrepareSeedForFile(ctx != nullptr ? ctx->GetSeedString() : std::string(), fileSeed);
    FleetOracle_QueueEnsureSave(slot);

    CVarSetInteger("gFleetCombo.LastSlot", slot);
    int startInMm = CVarGetInteger(("gFleetCombo.StartInMM.Slot" + std::to_string(slot)).c_str(), 0) ? 1 : 0;
    // Persist the start-in choice for this file.
    // NOTE (2026-07-31): this NO LONGER decides the boot game. FleetShipCombo_HostBootstrap now
    // always brings the combo up in OoT — booting straight into MM leaves OoT sitting on its file
    // select with no file loaded, which is the startup that kept crashing for testers (see the long
    // note there). The value is still recorded so the "start in MM" intent isn't lost if we bring
    // the feature back as a seamless in-session hand-off (the thing it always wanted to be), and
    // isPlayerIn2Ship keeps tracking the active game at runtime.
    CVarSetInteger("isPlayerIn2Ship", startInMm);
    CVarSave();
    SPDLOG_INFO("[FleetComboFS] combo save loaded from slot {} (startInMM={} persisted)", slot, startInMm);

    // If this combo was last saved in MM, OoT is just the doorway: load here, then hand the player
    // over automatically so they resume in MM's own save. No-op when the last save was in OoT.
    FleetCombo_QueueResumeToMm();
}

} // extern "C"
