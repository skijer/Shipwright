#include "FleetHints.h"

#include <initializer_list>
#include <string>
#include <unordered_set>
#include <utility>

#include "soh/Enhancements/randomizer/3drando/hints.hpp"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/randomizer/randomizerEnumStrings.h"
#include "soh/Enhancements/randomizer/randomizer_check_objects.h"
#include "soh/Enhancements/randomizer/static_data.h"
#include "soh/Enhancements/randomizer/trial.h"

namespace {

struct EndClause {
    uint32_t option;
    const char* hintTemplate;
    RandomizerSettingKey count;
};

// The altar template native picks for a setting, and the count it splices in.
std::pair<std::string, int> ResolveEndClause(RandomizerSettingKey setting, std::initializer_list<EndClause> clauses) {
    auto ctx = Rando::Context::GetInstance();
    for (const EndClause& clause : clauses) {
        if (ctx->GetOption(setting).Is(clause.option)) {
            const int count = clause.count == RSK_NONE ? 0 : ctx->GetOption(clause.count).Get();
            return { clause.hintTemplate, count };
        }
    }
    return { "", 0 };
}

std::pair<std::string, int> BridgeClause() {
    return ResolveEndClause(
        RSK_RAINBOW_BRIDGE,
        { { RO_BRIDGE_ALWAYS_OPEN, "RHT_BRIDGE_OPEN_HINT", RSK_NONE },
          { RO_BRIDGE_VANILLA, "RHT_BRIDGE_VANILLA_HINT", RSK_NONE },
          { RO_BRIDGE_STONES, "RHT_BRIDGE_STONES_HINT", RSK_RAINBOW_BRIDGE_STONE_COUNT },
          { RO_BRIDGE_MEDALLIONS, "RHT_BRIDGE_MEDALLIONS_HINT", RSK_RAINBOW_BRIDGE_MEDALLION_COUNT },
          { RO_BRIDGE_DUNGEON_REWARDS, "RHT_BRIDGE_REWARDS_HINT", RSK_RAINBOW_BRIDGE_REWARD_COUNT },
          { RO_BRIDGE_DUNGEONS, "RHT_BRIDGE_DUNGEONS_HINT", RSK_RAINBOW_BRIDGE_DUNGEON_COUNT },
          { RO_BRIDGE_TOKENS, "RHT_BRIDGE_TOKENS_HINT", RSK_RAINBOW_BRIDGE_TOKEN_COUNT },
          { RO_BRIDGE_TRIFORCE_PIECES, "RHT_BRIDGE_TRIFORCE_PIECES_HINT", RSK_RAINBOW_BRIDGE_TRIFORCE_COUNT },
          { RO_BRIDGE_GREG, "RHT_BRIDGE_GREG_HINT", RSK_NONE } });
}

std::pair<std::string, int> GanonBossKeyClause() {
    return ResolveEndClause(
        RSK_GANONS_BOSS_KEY,
        { { RO_GANON_BOSS_KEY_STARTWITH, "RHT_GANON_BK_START_WITH_HINT", RSK_NONE },
          { RO_GANON_BOSS_KEY_VANILLA, "RHT_GANON_BK_VANILLA_HINT", RSK_NONE },
          { RO_GANON_BOSS_KEY_OWN_DUNGEON, "RHT_GANON_BK_OWN_DUNGEON_HINT", RSK_NONE },
          { RO_GANON_BOSS_KEY_ANY_DUNGEON, "RHT_GANON_BK_ANY_DUNGEON_HINT", RSK_NONE },
          { RO_GANON_BOSS_KEY_OVERWORLD, "RHT_GANON_BK_OVERWORLD_HINT", RSK_NONE },
          { RO_GANON_BOSS_KEY_ANYWHERE, "RHT_GANON_BK_ANYWHERE_HINT", RSK_NONE },
          { RO_GANON_BOSS_KEY_STONES, "RHT_GBK_STONES_HINT", RSK_GBK_STONE_COUNT },
          { RO_GANON_BOSS_KEY_MEDALLIONS, "RHT_GBK_MEDALLIONS_HINT", RSK_GBK_MEDALLION_COUNT },
          { RO_GANON_BOSS_KEY_REWARDS, "RHT_GBK_REWARDS_HINT", RSK_GBK_REWARD_COUNT },
          { RO_GANON_BOSS_KEY_DUNGEONS, "RHT_GBK_DUNGEONS_HINT", RSK_GBK_DUNGEON_COUNT },
          { RO_GANON_BOSS_KEY_TOKENS, "RHT_GBK_TOKENS_HINT", RSK_GBK_TOKEN_COUNT },
          { RO_GANON_BOSS_KEY_TRIFORCE_PIECES, "RHT_GBK_TRIFORCE_PIECES_HINT", RSK_GBK_TRIFORCE_COUNT } });
}

std::pair<std::string, int> GanonsSoulClause() {
    return ResolveEndClause(
        RSK_GANONS_SOUL,
        { { RO_GANONS_SOUL_STONES, "RHT_GANONS_SOUL_STONES_HINT", RSK_GANONS_SOUL_STONE_COUNT },
          { RO_GANONS_SOUL_MEDALLIONS, "RHT_GANONS_SOUL_MEDALLIONS_HINT", RSK_GANONS_SOUL_MEDALLION_COUNT },
          { RO_GANONS_SOUL_REWARDS, "RHT_GANONS_SOUL_REWARDS_HINT", RSK_GANONS_SOUL_REWARD_COUNT },
          { RO_GANONS_SOUL_DUNGEONS, "RHT_GANONS_SOUL_DUNGEONS_HINT", RSK_GANONS_SOUL_DUNGEON_COUNT },
          { RO_GANONS_SOUL_TOKENS, "RHT_GANONS_SOUL_TOKENS_HINT", RSK_GANONS_SOUL_TOKEN_COUNT },
          { RO_GANONS_SOUL_TRIFORCE_PIECES, "RHT_GANONS_SOUL_TRIFORCE_PIECES_HINT", RSK_GANONS_SOUL_TRIFORCE_COUNT } });
}

std::pair<std::string, int> WinconClause() {
    return ResolveEndClause(
        RSK_WINCON,
        { { RO_WINCON_ANYWHERE, "RHT_WINCON_ANYWHERE_HINT", RSK_NONE },
          { RO_WINCON_STONES, "RHT_WINCON_STONES_HINT", RSK_WINCON_STONE_COUNT },
          { RO_WINCON_MEDALLIONS, "RHT_WINCON_MEDALLIONS_HINT", RSK_WINCON_MEDALLION_COUNT },
          { RO_WINCON_REWARDS, "RHT_WINCON_REWARDS_HINT", RSK_WINCON_REWARD_COUNT },
          { RO_WINCON_DUNGEONS, "RHT_WINCON_DUNGEONS_HINT", RSK_WINCON_DUNGEON_COUNT },
          { RO_WINCON_TOKENS, "RHT_WINCON_TOKENS_HINT", RSK_WINCON_TOKEN_COUNT },
          { RO_WINCON_TRIFORCE_PIECES, "RHT_WINCON_TRIFORCE_PIECES_HINT", RSK_WINCON_TRIFORCE_COUNT } });
}

const char* DoorOfTimeTemplate() {
    auto& doorOfTime = Rando::Context::GetInstance()->GetOption(RSK_DOOR_OF_TIME);
    if (doorOfTime.Is(RO_DOOROFTIME_OPEN)) {
        return "RHT_CHILD_ALTAR_TEXT_END_DOTOPEN";
    }
    if (doorOfTime.Is(RO_DOOROFTIME_SONGONLY)) {
        return "RHT_CHILD_ALTAR_TEXT_END_DOTSONGONLY";
    }
    return "RHT_CHILD_ALTAR_TEXT_END_DOTCLOSED";
}

int OptionValue(RandomizerSettingKey key) {
    return Rando::Context::GetInstance()->GetOption(key).Get();
}

nlohmann::json DumpOptions() {
    const auto [bridgeTemplate, bridgeCount] = BridgeClause();
    const auto [gbkTemplate, gbkCount] = GanonBossKeyClause();
    const auto [soulTemplate, soulCount] = GanonsSoulClause();
    const auto [winconTemplate, winconCount] = WinconClause();
    return { { "gossipStoneHints", OptionValue(RSK_GOSSIP_STONE_HINTS) },
             { "hintClarity", OptionValue(RSK_HINT_CLARITY) },
             { "hintDistribution", OptionValue(RSK_HINT_DISTRIBUTION) },
             { "ganondorfHint", OptionValue(RSK_GANONDORF_HINT) },
             { "shuffleMasterSword", OptionValue(RSK_SHUFFLE_MASTER_SWORD) },
             { "startingMasterSword", OptionValue(RSK_STARTING_MASTER_SWORD) },
             { "warpSongHints", OptionValue(RSK_WARP_SONG_HINTS) },
             { "totAltarHint", OptionValue(RSK_TOT_ALTAR_HINT) },
             { "sheikLaHint", OptionValue(RSK_SHEIK_LA_HINT) },
             { "bossKeyHint", OptionValue(RSK_BOSS_KEY_HINT) },
             { "dampesDiaryHint", OptionValue(RSK_DAMPES_DIARY_HINT) },
             { "gregHint", OptionValue(RSK_GREG_HINT) },
             { "sariaHint", OptionValue(RSK_SARIA_HINT) },
             { "midoHint", OptionValue(RSK_MIDO_HINT) },
             { "fishingPoleHint", OptionValue(RSK_FISHING_POLE_HINT) },
             { "doorOfTimeTemplate", DoorOfTimeTemplate() },
             { "bridgeTemplate", bridgeTemplate },
             { "bridgeCount", bridgeCount },
             { "gbkTemplate", gbkTemplate },
             { "gbkCount", gbkCount },
             { "soulTemplate", soulTemplate },
             { "soulCount", soulCount },
             { "winconTemplate", winconTemplate },
             { "winconCount", winconCount } };
}

nlohmann::json MessageJson(const CustomMessage& message) {
    return { { "en", message.GetEnglish(MF_ENCODE) },
             { "de", message.GetGerman(MF_ENCODE) },
             { "fr", message.GetFrench(MF_ENCODE) } };
}

nlohmann::json HintTextJson(const HintText& hint) {
    nlohmann::json ambiguous = nlohmann::json::array();
    for (size_t i = 0; i < hint.GetAmbiguousSize(); i++) {
        ambiguous.push_back(MessageJson(hint.GetAmbiguous(i)));
    }
    nlohmann::json obscure = nlohmann::json::array();
    for (size_t i = 0; i < hint.GetObscureSize(); i++) {
        obscure.push_back(MessageJson(hint.GetObscure(i)));
    }
    return { { "clear", MessageJson(hint.GetClear()) },
             { "ambiguous", std::move(ambiguous) },
             { "obscure", std::move(obscure) } };
}

void PushCheckName(nlohmann::json& names, RandomizerCheck rc) {
    const std::string& name = Rando::StaticData::GetLocation(rc)->GetName();
    if (!name.empty()) {
        names.push_back(name);
    }
}

nlohmann::json DumpAreas() {
    nlohmann::json areas = nlohmann::json::array();
    for (int key = 0; key < RCAREA_INVALID; key++) {
        const auto area = static_cast<RandomizerCheckArea>(key);
        areas.push_back({ { "key", key },
                          { "name", RandomizerCheckObjects::GetRCAreaName(area) },
                          { "dungeon", RandomizerCheckObjects::AreaIsDungeon(area) } });
    }
    return areas;
}

// The checks a player already knows at the start, which native stone hints never target.
nlohmann::json DumpHintAccessibleChecks() {
    auto ctx = Rando::Context::GetInstance();
    nlohmann::json known = nlohmann::json::array();
    if (ctx->GetOption(RSK_STARTING_ZELDAS_LETTER) && !ctx->GetOption(RSK_SHUFFLE_ZELDAS_LETTER)) {
        PushCheckName(known, RC_SONG_FROM_IMPA);
    }
    if (ctx->GetOption(RSK_SELECTED_STARTING_AGE).Is(RO_AGE_ADULT) || !ctx->GetOption(RSK_SHUFFLE_MASTER_SWORD)) {
        PushCheckName(known, RC_TOT_MASTER_SWORD);
    }
    return known;
}

nlohmann::json DumpCheckHints() {
    nlohmann::json checks = nlohmann::json::array();
    for (int rc = 0; rc < RC_MAX; rc++) {
        Rando::Location* location = Rando::StaticData::GetLocation(static_cast<RandomizerCheck>(rc));
        if (location == nullptr || location->GetName().empty()) {
            continue;
        }
        checks.push_back({ { "name", location->GetName() },
                           { "area", RandomizerCheckObjects::GetRCAreaName(location->GetArea()) },
                           { "dungeon", location->IsDungeon() },
                           { "overworld", location->IsOverworld() },
                           { "song", location->GetRCType() == RCTYPE_SONG_LOCATION },
                           { "locationHint", HintTextJson(*location->GetHint()) } });
    }
    return checks;
}

nlohmann::json DumpItemHints() {
    nlohmann::json items = nlohmann::json::array();
    for (int rg = 0; rg < RG_MAX; rg++) {
        const Rando::Item& item = Rando::StaticData::RetrieveItem(static_cast<RandomizerGet>(rg));
        const std::string& name = item.GetName().GetEnglish();
        if (!name.empty()) {
            items.push_back({ { "name", name }, { "hint", HintTextJson(item.GetHint()) } });
        }
    }
    return items;
}

// Only the templates the cross-game composer can emit; the full table is ~1600 entries.
bool IsComposerTemplate(const std::string& name) {
    static const std::unordered_set<std::string> templates = {
        "RHT_WAY_OF_THE_HERO",
        "RHT_FOOLISH",
        "RHT_CAN_BE_FOUND_AT",
        "RHT_HOARDS",
        "RHT_GANONDORF_HINT_LA_ONLY",
        "RHT_GANONDORF_HINT_MS_ONLY",
        "RHT_GANONDORF_HINT_LA_AND_MS",
        "RHT_YOUR_POCKET",
        "RHT_SHEIK_HINT_LA_ONLY",
        "RHT_BOSS_KEY_HINT",
        "RHT_DAMPE_DIARY",
        "RHT_GREG_HINT",
        "RHT_SARIA_TALK_HINT",
        "RHT_SARIA_SONG_HINT",
        "RHT_MIDO_HINT",
        "RHT_FISHING_POLE_HINT",
        "RHT_CHILD_ALTAR_STONES",
        "RHT_CHILD_ALTAR_TEXT_END_DOTOPEN",
        "RHT_CHILD_ALTAR_TEXT_END_DOTSONGONLY",
        "RHT_CHILD_ALTAR_TEXT_END_DOTCLOSED",
        "RHT_ADULT_ALTAR_MEDALLIONS",
        "RHT_ADULT_ALTAR_TEXT_END",
        "RHT_BRIDGE_OPEN_HINT",
        "RHT_BRIDGE_VANILLA_HINT",
        "RHT_BRIDGE_STONES_HINT",
        "RHT_BRIDGE_MEDALLIONS_HINT",
        "RHT_BRIDGE_REWARDS_HINT",
        "RHT_BRIDGE_DUNGEONS_HINT",
        "RHT_BRIDGE_TOKENS_HINT",
        "RHT_BRIDGE_TRIFORCE_PIECES_HINT",
        "RHT_BRIDGE_GREG_HINT",
        "RHT_GANON_BK_START_WITH_HINT",
        "RHT_GANON_BK_VANILLA_HINT",
        "RHT_GANON_BK_OWN_DUNGEON_HINT",
        "RHT_GANON_BK_ANY_DUNGEON_HINT",
        "RHT_GANON_BK_OVERWORLD_HINT",
        "RHT_GANON_BK_ANYWHERE_HINT",
        "RHT_GBK_STONES_HINT",
        "RHT_GBK_MEDALLIONS_HINT",
        "RHT_GBK_REWARDS_HINT",
        "RHT_GBK_DUNGEONS_HINT",
        "RHT_GBK_TOKENS_HINT",
        "RHT_GBK_TRIFORCE_PIECES_HINT",
        "RHT_GANONS_SOUL_STONES_HINT",
        "RHT_GANONS_SOUL_MEDALLIONS_HINT",
        "RHT_GANONS_SOUL_REWARDS_HINT",
        "RHT_GANONS_SOUL_DUNGEONS_HINT",
        "RHT_GANONS_SOUL_TOKENS_HINT",
        "RHT_GANONS_SOUL_TRIFORCE_PIECES_HINT",
        "RHT_WINCON_ANYWHERE_HINT",
        "RHT_WINCON_STONES_HINT",
        "RHT_WINCON_MEDALLIONS_HINT",
        "RHT_WINCON_REWARDS_HINT",
        "RHT_WINCON_DUNGEONS_HINT",
        "RHT_WINCON_TOKENS_HINT",
        "RHT_WINCON_TRIFORCE_PIECES_HINT",
    };
    return templates.contains(name) || name.starts_with("RHT_JUNK");
}

nlohmann::json DumpHintTemplates() {
    nlohmann::json templates = nlohmann::json::object();
    for (int key = 0; key < RHT_MAX; key++) {
        const std::string name(EnumToString(static_cast<RandomizerHintTextKey>(key)));
        if (!name.empty() && IsComposerTemplate(name)) {
            templates[name] = HintTextJson(Rando::StaticData::hintTextTable[key]);
        }
    }
    return templates;
}

nlohmann::json DumpRequiredTrials() {
    nlohmann::json trials = nlohmann::json::array();
    for (Rando::TrialInfo* trial : Rando::Context::GetInstance()->GetTrials()->GetTrialList()) {
        if (trial->IsRequired()) {
            trials.push_back(trial->GetName().GetEnglish(MF_RAW));
        }
    }
    return trials;
}

} // namespace

nlohmann::json FleetHints_DumpHintData() {
    nlohmann::json stones = nlohmann::json::array();
    for (RandomizerCheck rc : Rando::StaticData::GetGossipStoneLocations()) {
        PushCheckName(stones, rc);
    }
    nlohmann::json alwaysHinted = nlohmann::json::array();
    for (RandomizerCheck rc : GetAlwaysHintCandidates()) {
        PushCheckName(alwaysHinted, rc);
    }
    return { { "options", DumpOptions() },
             { "areas", DumpAreas() },
             { "stones", std::move(stones) },
             { "alwaysHintChecks", std::move(alwaysHinted) },
             { "hintAccessibleChecks", DumpHintAccessibleChecks() },
             { "checks", DumpCheckHints() },
             { "items", DumpItemHints() },
             { "hintTextTable", DumpHintTemplates() },
             { "requiredTrials", DumpRequiredTrials() } };
}
