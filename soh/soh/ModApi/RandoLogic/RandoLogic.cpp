#include "soh/ModApi/RandoLogic/RandoLogic.h"

#include <algorithm>
#include <array>
#include <list>
#include <string>
#include <unordered_map>
#include <vector>

#include <spdlog/spdlog.h>

#include "soh/Enhancements/randomizer/location_access.h"
#include "soh/Enhancements/randomizer/entrance.h"
#include "soh/Enhancements/randomizer/static_data.h"
#include "soh/ModApi/ModDungeons/DungeonRegistry.h"

bool gRandoLogicHasCapabilityGrants = false;
bool gRandoLogicHasItemGrants = false;
bool gRandoLogicHasEnemyGrants = false;

namespace {

const char* const kCapabilityNames[RLC_MAX] = {
#define RANDO_LOGIC_CAPABILITY_NAME(name) #name,
    RANDO_LOGIC_CAPABILITIES(RANDO_LOGIC_CAPABILITY_NAME)
#undef RANDO_LOGIC_CAPABILITY_NAME
};

struct Selector {
    uint32_t targets;
    std::string capabilityCall;
    std::string conditionText;
    int32_t checkType;
    int32_t region;
    int32_t scene;
    int32_t entranceTo;
    int32_t logicEvent;
    std::vector<int32_t> checks;
};

struct Rule {
    std::string name;
    SOHLogicEffect effect;
    SOHLogicCondition condition;
    void* userData;
    std::vector<Selector> include;
    std::vector<Selector> exclude;
    bool evaluating;
};

struct WorldEntry {
    uint32_t target;
    int32_t id;
    int32_t region;
    int32_t scene;
    int32_t entranceTo;
    int32_t checkType;
    const std::string* condition;
};

struct Grant {
    std::string owner;
    SOHLogicCondition condition;
    void* userData;
    std::vector<Selector> include;
    std::vector<Selector> exclude;
    bool evaluating;
};

std::array<std::vector<Grant>, RLC_MAX> sCapabilityGrants;
std::array<bool, RLC_MAX> sCapabilityEvaluating{};
std::unordered_map<int32_t, std::vector<Grant>> sItemGrants;
std::unordered_map<int32_t, std::vector<Grant>> sEnemyGrants;
// The check, entrance or event whose condition is being evaluated, innermost last.
std::vector<WorldEntry> sEntryStack;
std::list<Rule> sRules;

std::vector<std::vector<Rule*>> sLocationRules;
std::vector<std::vector<Rule*>> sEventRules;
std::unordered_map<uint64_t, std::vector<Rule*>> sEntranceRules;
bool sHasLocationRules = false;
bool sHasEventRules = false;
bool sWorldBuilt = false;

uint64_t EntranceKey(int32_t fromRegion, int32_t toRegion) {
    return (static_cast<uint64_t>(static_cast<uint32_t>(fromRegion)) << 32) | static_cast<uint32_t>(toRegion);
}

int32_t FindCapability(const char* capability) {
    if (capability == nullptr) {
        return -1;
    }
    for (int32_t index = 0; index < RLC_MAX; index++) {
        if (std::string(kCapabilityNames[index]) == capability) {
            return index;
        }
    }
    return -1;
}

Rule* FindRule(const std::string& name) {
    for (Rule& rule : sRules) {
        if (rule.name == name) {
            return &rule;
        }
    }
    return nullptr;
}

Selector ReadSelector(const SOHLogicSelector& source) {
    Selector selector{};
    selector.targets = source.targets;
    if (source.capability != nullptr) {
        selector.capabilityCall = std::string(source.capability) + "(";
    }
    if (source.conditionText != nullptr) {
        selector.conditionText = source.conditionText;
    }
    selector.checkType = source.checkType;
    selector.region = source.region;
    selector.scene = source.scene;
    selector.entranceTo = source.entranceTo;
    selector.logicEvent = source.logicEvent;
    for (uint32_t index = 0; index < source.checkCount; index++) {
        selector.checks.push_back(source.checks[index]);
    }
    return selector;
}

bool MatchesSelector(const Selector& selector, const WorldEntry& entry) {
    if (selector.targets != SOH_LOGIC_TARGET_ALL && (selector.targets & entry.target) == 0) {
        return false;
    }
    if (selector.region >= 0 && selector.region != entry.region) {
        return false;
    }
    if (selector.scene >= 0 && selector.scene != entry.scene) {
        return false;
    }
    if (selector.entranceTo >= 0 && selector.entranceTo != entry.entranceTo) {
        return false;
    }
    if (selector.checkType >= 0 && selector.checkType != entry.checkType) {
        return false;
    }
    if (selector.logicEvent >= 0 && (entry.target != SOH_LOGIC_TARGET_EVENTS || selector.logicEvent != entry.id)) {
        return false;
    }
    if (!selector.checks.empty()) {
        if (entry.target != SOH_LOGIC_TARGET_LOCATIONS) {
            return false;
        }
        if (std::find(selector.checks.begin(), selector.checks.end(), entry.id) == selector.checks.end()) {
            return false;
        }
    }
    if (!selector.capabilityCall.empty() && entry.condition->find(selector.capabilityCall) == std::string::npos) {
        return false;
    }
    if (!selector.conditionText.empty() && entry.condition->find(selector.conditionText) == std::string::npos) {
        return false;
    }
    return true;
}

bool MatchesAny(const std::vector<Selector>& selectors, const WorldEntry& entry) {
    for (const Selector& selector : selectors) {
        if (MatchesSelector(selector, entry)) {
            return true;
        }
    }
    return false;
}

bool SelectsEntry(const Rule& rule, const WorldEntry& entry) {
    if (!MatchesAny(rule.include, entry)) {
        return false;
    }
    return !MatchesAny(rule.exclude, entry);
}

void AddRuleToBucket(std::vector<Rule*>& bucket, Rule* rule) {
    if (bucket.empty() || bucket.back() != rule) {
        bucket.push_back(rule);
    }
}

int32_t GetCheckType(int32_t check) {
    Rando::Location* location = Rando::StaticData::GetLocation(static_cast<RandomizerCheck>(check));
    if (location == nullptr) {
        return -1;
    }
    return location->GetRCType();
}

void IndexRule(Rule& rule) {
    for (uint32_t regionKey = RR_NONE + 1; regionKey < RR_MAX; regionKey++) {
        Region& region = areaTable[regionKey];
        WorldEntry entry{};
        entry.region = static_cast<int32_t>(regionKey);
        entry.scene = static_cast<int32_t>(region.scene);

        entry.target = SOH_LOGIC_TARGET_LOCATIONS;
        entry.entranceTo = -1;
        for (const LocationAccess& location : region.locations) {
            entry.id = static_cast<int32_t>(location.GetLocation());
            if (entry.id < 0 || entry.id >= RC_MAX) {
                continue;
            }
            entry.checkType = GetCheckType(entry.id);
            const std::string conditionStr = location.GetConditionStr();
            entry.condition = &conditionStr;
            if (SelectsEntry(rule, entry)) {
                AddRuleToBucket(sLocationRules[entry.id], &rule);
            }
        }

        entry.target = SOH_LOGIC_TARGET_EVENTS;
        entry.checkType = -1;
        for (const EventAccess& event : region.events) {
            entry.id = static_cast<int32_t>(event.GetEventKey());
            if (entry.id < 0 || entry.id >= LOGIC_MAX) {
                continue;
            }
            entry.condition = &event.GetConditionStr();
            if (SelectsEntry(rule, entry)) {
                AddRuleToBucket(sEventRules[entry.id], &rule);
            }
        }

        entry.target = SOH_LOGIC_TARGET_ENTRANCES;
        entry.id = -1;
        entry.checkType = -1;
        for (const Rando::Entrance& exit : region.exits) {
            entry.entranceTo = static_cast<int32_t>(exit.GetOriginalConnectedRegionKey());
            entry.condition = &exit.GetConditionStr();
            if (SelectsEntry(rule, entry)) {
                AddRuleToBucket(sEntranceRules[EntranceKey(entry.region, entry.entranceTo)], &rule);
            }
        }
    }
}

bool HoldsCondition(Rule& rule) {
    if (rule.condition == nullptr) {
        return true;
    }
    if (rule.evaluating) {
        return false;
    }
    rule.evaluating = true;
    bool holds = rule.condition(rule.userData);
    rule.evaluating = false;
    return holds;
}

bool ApplyRules(const std::vector<Rule*>& rules, bool vanillaResult) {
    bool result = vanillaResult;
    for (Rule* rule : rules) {
        switch (rule->effect) {
            case SOH_LOGIC_ALLOW:
                result = result || HoldsCondition(*rule);
                break;
            case SOH_LOGIC_REQUIRE:
                result = result && HoldsCondition(*rule);
                break;
            case SOH_LOGIC_DENY:
                result = false;
                break;
            case SOH_LOGIC_REPLACE:
                result = HoldsCondition(*rule);
                break;
            default:
                SPDLOG_ERROR("Logic rule {} has unknown effect {}", rule->name, static_cast<int>(rule->effect));
                break;
        }
    }
    return result;
}

void ReindexWhenWorldBuilt() {
    if (sWorldBuilt) {
        RandoLogic_IndexWorld();
    }
}

// Outside any check, entrance or event (a helper asked directly) only unscoped grants apply.
bool IsInScope(const Grant& grant) {
    if (sEntryStack.empty()) {
        return grant.include.empty();
    }
    const WorldEntry& entry = sEntryStack.back();
    if (!grant.include.empty() && !MatchesAny(grant.include, entry)) {
        return false;
    }
    return !MatchesAny(grant.exclude, entry);
}

// A grant whose condition asks for what it grants answers false to itself instead of recursing.
bool AnyGrantHolds(std::vector<Grant>& grants) {
    for (Grant& grant : grants) {
        if (grant.evaluating || !IsInScope(grant)) {
            continue;
        }
        grant.evaluating = true;
        bool holds = grant.condition(grant.userData);
        grant.evaluating = false;
        if (holds) {
            return true;
        }
    }
    return false;
}

bool AnyMappedGrantHolds(std::unordered_map<int32_t, std::vector<Grant>>& grants, int32_t key) {
    auto found = grants.find(key);
    return found != grants.end() && AnyGrantHolds(found->second);
}

Grant ReadGrant(const SOHLogicGrant& source) {
    Grant grant{};
    grant.owner = source.owner;
    grant.condition = source.condition;
    grant.userData = source.userData;
    for (uint32_t index = 0; index < source.includeCount; index++) {
        grant.include.push_back(ReadSelector(source.include[index]));
    }
    for (uint32_t index = 0; index < source.excludeCount; index++) {
        grant.exclude.push_back(ReadSelector(source.exclude[index]));
    }
    return grant;
}

void RemoveOwnerGrants(std::vector<Grant>& grants, const std::string& owner) {
    grants.erase(std::remove_if(grants.begin(), grants.end(), [&owner](const Grant& grant) { return grant.owner == owner; }),
                 grants.end());
}

class EntryScope {
  public:
    explicit EntryScope(const WorldEntry& entry) {
        sEntryStack.push_back(entry);
    }
    ~EntryScope() {
        sEntryStack.pop_back();
    }
};

int32_t GetCheckScene(int32_t check) {
    if (check < 0 || check >= RC_MAX) {
        return -1;
    }
    Rando::Location* location = Rando::StaticData::GetLocation(static_cast<RandomizerCheck>(check));
    return location != nullptr ? static_cast<int32_t>(location->GetScene()) : -1;
}

bool ResolveLocation(int32_t check, bool vanillaResult) {
    if (!sHasLocationRules || check < 0 || check >= RC_MAX) {
        return vanillaResult;
    }
    return ApplyRules(sLocationRules[check], vanillaResult);
}

bool ResolveEntrance(int32_t fromRegion, int32_t toRegion, bool vanillaResult) {
    if (sEntranceRules.empty()) {
        return vanillaResult;
    }
    auto found = sEntranceRules.find(EntranceKey(fromRegion, toRegion));
    if (found == sEntranceRules.end()) {
        return vanillaResult;
    }
    return ApplyRules(found->second, vanillaResult);
}

bool ResolveEvent(int32_t logicEvent, bool vanillaResult) {
    if (!sHasEventRules || logicEvent < 0 || logicEvent >= LOGIC_MAX) {
        return vanillaResult;
    }
    return ApplyRules(sEventRules[logicEvent], vanillaResult);
}

} // namespace

bool RandoLogic_GrantCapability(const char* owner, const char* capability, SOHLogicCondition condition,
                                void* userData) {
    int32_t index = FindCapability(capability);
    if (index < 0 || owner == nullptr || condition == nullptr) {
        SPDLOG_ERROR("Rejected a grant of logic capability {}", capability != nullptr ? capability : "(null)");
        return false;
    }
    Grant grant{};
    grant.owner = owner;
    grant.condition = condition;
    grant.userData = userData;
    sCapabilityGrants[index].push_back(grant);
    gRandoLogicHasCapabilityGrants = true;
    return true;
}

bool RandoLogic_Grant(const SOHLogicGrant* definition) {
    if (definition == nullptr || definition->structSize < sizeof(SOHLogicGrant) || definition->owner == nullptr ||
        definition->condition == nullptr) {
        SPDLOG_ERROR("Rejected a logic grant with no owner, no condition or an older struct");
        return false;
    }
    switch (definition->kind) {
        case SOH_LOGIC_GRANT_CAPABILITY: {
            int32_t index = FindCapability(definition->capability);
            if (index < 0) {
                SPDLOG_ERROR("{}: no logic capability {}", definition->owner,
                             definition->capability != nullptr ? definition->capability : "(null)");
                return false;
            }
            sCapabilityGrants[index].push_back(ReadGrant(*definition));
            gRandoLogicHasCapabilityGrants = true;
            return true;
        }
        case SOH_LOGIC_GRANT_ITEM:
            if (definition->value <= RG_NONE) {
                SPDLOG_ERROR("{}: a logic item grant needs a RandomizerGet", definition->owner);
                return false;
            }
            sItemGrants[definition->value].push_back(ReadGrant(*definition));
            gRandoLogicHasItemGrants = true;
            return true;
        case SOH_LOGIC_GRANT_ENEMY_KILL:
            sEnemyGrants[definition->value].push_back(ReadGrant(*definition));
            gRandoLogicHasEnemyGrants = true;
            return true;
        default:
            SPDLOG_ERROR("{}: unknown logic grant kind {}", definition->owner, static_cast<int>(definition->kind));
            return false;
    }
}

bool RandoLogic_RegisterRule(const SOHLogicRule* definition) {
    if (definition == nullptr || definition->structSize < sizeof(SOHLogicRule) || definition->name == nullptr) {
        SPDLOG_ERROR("Rejected a logic rule with no name or an older struct");
        return false;
    }
    if (definition->includeCount == 0) {
        SPDLOG_ERROR("Logic rule {} selects nothing", definition->name);
        return false;
    }

    Rule rule{};
    rule.name = definition->name;
    rule.effect = definition->effect;
    rule.condition = definition->condition;
    rule.userData = definition->userData;
    for (uint32_t index = 0; index < definition->includeCount; index++) {
        rule.include.push_back(ReadSelector(definition->include[index]));
    }
    for (uint32_t index = 0; index < definition->excludeCount; index++) {
        rule.exclude.push_back(ReadSelector(definition->exclude[index]));
    }

    Rule* existing = FindRule(rule.name);
    if (existing != nullptr) {
        *existing = rule;
    } else {
        sRules.push_back(rule);
    }
    ReindexWhenWorldBuilt();
    return true;
}

void RandoLogic_RemoveOwner(const char* name) {
    if (name == nullptr) {
        return;
    }
    std::string owner = name;
    sRules.remove_if([&owner](const Rule& rule) { return rule.name == owner; });
    for (std::vector<Grant>& grants : sCapabilityGrants) {
        RemoveOwnerGrants(grants, owner);
    }
    for (auto& [item, grants] : sItemGrants) {
        RemoveOwnerGrants(grants, owner);
    }
    for (auto& [enemy, grants] : sEnemyGrants) {
        RemoveOwnerGrants(grants, owner);
    }
    ReindexWhenWorldBuilt();
}

void RandoLogic_IndexWorld() {
    sWorldBuilt = true;
    sLocationRules.assign(RC_MAX, {});
    sEventRules.assign(LOGIC_MAX, {});
    sEntranceRules.clear();
    sHasLocationRules = false;
    sHasEventRules = false;
    for (Rule& rule : sRules) {
        IndexRule(rule);
    }
    for (const std::vector<Rule*>& bucket : sLocationRules) {
        sHasLocationRules = sHasLocationRules || !bucket.empty();
    }
    for (const std::vector<Rule*>& bucket : sEventRules) {
        sHasEventRules = sHasEventRules || !bucket.empty();
    }
}

bool RandoLogic_IsCapabilityGranted(RandoLogicCapability capability) {
    if (capability >= RLC_MAX || sCapabilityGrants[capability].empty() || sCapabilityEvaluating[capability]) {
        return false;
    }
    sCapabilityEvaluating[capability] = true;
    bool granted = AnyGrantHolds(sCapabilityGrants[capability]);
    sCapabilityEvaluating[capability] = false;
    return granted;
}

bool RandoLogic_IsItemGranted(int32_t randoGet) {
    return AnyMappedGrantHolds(sItemGrants, randoGet);
}

bool RandoLogic_IsEnemyKillGranted(int32_t enemy) {
    return AnyMappedGrantHolds(sEnemyGrants, enemy);
}

// Mod checks, exits and events answer with their own condition language, never with the vanilla one.
bool RandoLogic_EvaluateLocation(int32_t check, const std::string& condition, const std::function<bool()>& evaluate) {
    if (DungeonRegistry_IsModCheck(check)) {
        return DungeonRegistry_EvaluateCheck(check);
    }
    WorldEntry entry{};
    entry.target = SOH_LOGIC_TARGET_LOCATIONS;
    entry.id = check;
    entry.region = -1;
    entry.scene = GetCheckScene(check);
    entry.entranceTo = -1;
    entry.checkType = entry.scene >= 0 ? GetCheckType(check) : -1;
    entry.condition = &condition;
    EntryScope scope(entry);
    return ResolveLocation(check, evaluate());
}

bool RandoLogic_EvaluateEntrance(int32_t fromRegion, int32_t toRegion, const std::string& condition,
                                 const std::function<bool()>& evaluate) {
    if (fromRegion >= RR_MAX || toRegion >= RR_MAX) {
        return DungeonRegistry_EvaluateExit(fromRegion, toRegion);
    }
    WorldEntry entry{};
    entry.target = SOH_LOGIC_TARGET_ENTRANCES;
    entry.id = -1;
    entry.region = fromRegion;
    entry.scene = fromRegion > RR_NONE ? static_cast<int32_t>(areaTable[fromRegion].scene) : -1;
    entry.entranceTo = toRegion;
    entry.checkType = -1;
    entry.condition = &condition;
    EntryScope scope(entry);
    return ResolveEntrance(fromRegion, toRegion, evaluate());
}

bool RandoLogic_EvaluateEvent(int32_t logicEvent, const std::string& condition, const std::function<bool()>& evaluate) {
    if (DungeonRegistry_IsModEvent(logicEvent)) {
        return DungeonRegistry_EvaluateEvent(logicEvent);
    }
    WorldEntry entry{};
    entry.target = SOH_LOGIC_TARGET_EVENTS;
    entry.id = logicEvent;
    entry.region = -1;
    entry.scene = -1;
    entry.entranceTo = -1;
    entry.checkType = -1;
    entry.condition = &condition;
    EntryScope scope(entry);
    return ResolveEvent(logicEvent, evaluate());
}

bool RandoLogic_CanUseItem(int32_t randoGet) {
    return logic != nullptr && logic->CanUse(static_cast<RandomizerGet>(randoGet));
}

bool RandoLogic_HasLogicItem(int32_t randoGet) {
    return logic != nullptr && logic->HasItem(static_cast<RandomizerGet>(randoGet));
}

bool RandoLogic_IsChild(void) {
    return logic != nullptr && logic->IsChild;
}

bool RandoLogic_IsAdult(void) {
    return logic != nullptr && logic->IsAdult;
}

bool RandoLogic_IsAtDay(void) {
    return logic != nullptr && logic->AtDay;
}

bool RandoLogic_IsAtNight(void) {
    return logic != nullptr && logic->AtNight;
}

bool RandoLogic_HasCapability(const char* capability) {
    int32_t index = FindCapability(capability);
    if (index < 0 || logic == nullptr) {
        return false;
    }
    switch (static_cast<RandoLogicCapability>(index)) {
#define RANDO_LOGIC_CAPABILITY_CALL(name) \
    case RLC_##name:                      \
        return logic->name();
        RANDO_LOGIC_CAPABILITIES(RANDO_LOGIC_CAPABILITY_CALL)
#undef RANDO_LOGIC_CAPABILITY_CALL
        default:
            return false;
    }
}
