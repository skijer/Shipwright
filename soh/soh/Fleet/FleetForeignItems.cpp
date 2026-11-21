#include "FleetForeignItems.h"
#include "FleetItemDraw.h"

#include <stdexcept>
#include <unordered_map>

#include "soh/ModApi/CustomItemRegistry/CustomItemRegistry.h"
#include "soh/Enhancements/randomizer/static_data.h"

extern "C" {
#include "functions.h"
}

namespace {

constexpr const char* kGame = "oot";

struct ForeignItem {
    std::string owner;
    std::string item;
};

std::unordered_map<std::string, ForeignItem> sItems;
// Entries are never erased: the registry and the frame's display list point into them.
std::unordered_map<std::string, std::string> sIconOwners;

const char* RecordTextboxIcon(const ForeignItem& foreign) {
    const std::string path = FleetItemDraw_ForeignTextboxIconPath(foreign.owner.c_str(), foreign.item.c_str());
    if (path.empty()) {
        return nullptr;
    }
    return sIconOwners.emplace(path, foreign.owner).first->first.c_str();
}

std::string PresentedName(const nlohmann::json& foreign) {
    const std::string fake = foreign.value("fakeItemName", "");
    return fake.empty() ? foreign.at("itemName").get<std::string>() : fake;
}

GetItemCategory ChestCategory(const std::string& category) {
    static const std::unordered_map<std::string, GetItemCategory> categories = {
        { "major", ITEM_CATEGORY_MAJOR },        { "mask", ITEM_CATEGORY_MAJOR },
        { "smallKey", ITEM_CATEGORY_SMALL_KEY }, { "bossKey", ITEM_CATEGORY_BOSS_KEY },
        { "health", ITEM_CATEGORY_HEALTH },      { "token", ITEM_CATEGORY_SKULLTULA_TOKEN },
    };
    const auto found = categories.find(category);
    return found != categories.end() ? found->second : ITEM_CATEGORY_LESSER;
}

void DrawForeignItem(PlayState* play, GetItemEntry* entry) {
    const SOHCustomItemDefinition* definition = CustomItemRegistry_FindByRuntimeId(entry->tableId);
    const auto found = definition != nullptr ? sItems.find(definition->key) : sItems.end();
    if (found != sItems.end() &&
        FleetItemDraw_DrawForeign(play, found->second.owner.c_str(), found->second.item.c_str())) {
        return;
    }
    GetItem_Draw(play, GID_RUPEE_BLUE);
}

void Register(const std::string& key, const ForeignItem& foreign, const nlohmann::json& reward) {
    const bool shared = reward.value("shared", false);
    const bool disguised = !reward.value("fakeItemName", std::string{}).empty();
    const std::string shown = disguised ? foreign.item : reward.value("label", foreign.item);
    const std::string text = "You found %y" + shown + (shared ? " (Shared)" : " (MM)") + "%w!&" +
                             (shared ? "Both games share it." : "It belongs to Majora's Mask.");
    SOHCustomItemRandomizer randomizer = {};
    randomizer.structSize = sizeof(randomizer);
    randomizer.flags = reward.value("advancement", false) ? SOH_CUSTOM_ITEM_RANDO_ADVANCEMENT : 0;
    SOHCustomItemDefinition definition = {};
    definition.structSize = sizeof(definition);
    definition.key = key.c_str();
    definition.randomizer = &randomizer;
    definition.presentationFlags = SOH_ITEM_HIDE_FROM_SAVE_EDITOR | SOH_ITEM_HIDE_FROM_KALEIDO;
    definition.getItemText = text.c_str();
    definition.displayName = foreign.item.c_str();
    definition.iconPath = RecordTextboxIcon(foreign);
    definition.disabledIconSurfaces = ~static_cast<uint32_t>(SOH_ITEM_ICON_TEXTBOX);
    definition.getItemEntry.getItemCategory = ChestCategory(reward.value("category", ""));
    definition.getItemEntry.drawFunc = DrawForeignItem;
    if (!CustomItemRegistry_Register(&definition)) {
        throw std::runtime_error("could not register the foreign item " + key);
    }
}

} // namespace

std::string FleetForeignItems_Key(const nlohmann::json& foreign) {
    return "fleet:" + foreign.at("itemGame").get<std::string>() + ":" + PresentedName(foreign);
}

std::string FleetForeignItems_KeyForCheck(const nlohmann::json& seed, const std::string& check) {
    for (const auto& foreign : seed.at("foreign")) {
        if (foreign.at("checkGame") == kGame && foreign.at("checkName") == check) {
            return FleetForeignItems_Key(foreign);
        }
    }
    return "";
}

void FleetForeignItems_Register(const nlohmann::json& seed) {
    for (const auto& foreign : seed.at("foreign")) {
        if (foreign.at("checkGame") != kGame) {
            continue;
        }
        const std::string key = FleetForeignItems_Key(foreign);
        if (sItems.contains(key)) {
            continue;
        }
        const ForeignItem item = { foreign.at("itemGame").get<std::string>(), PresentedName(foreign) };
        Register(key, item, foreign);
        sItems.emplace(key, item);
    }
}

const char* FleetForeignItems_FindIconOwner(const char* iconPath) {
    if (iconPath == nullptr || sIconOwners.empty()) {
        return nullptr;
    }
    const auto found = sIconOwners.find(iconPath);
    return found != sIconOwners.end() ? found->second.c_str() : nullptr;
}
