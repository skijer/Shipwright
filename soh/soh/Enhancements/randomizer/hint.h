#pragma once

#include "3drando/hints.hpp"
#include "../custom-message/CustomMessageManager.h"
#include "randomizerTypes.h"
#include <vector>
#include "nlohmann/json.hpp"
using oJson = nlohmann::ordered_json;

namespace Rando {
class Hint {
  public:
    Hint();
    Hint(RandomizerHint ownKey_, HintType hintType_, std::string distributionName_,
         std::vector<RandomizerHintTextKey> hintKeys_, std::vector<RandomizerCheck> locations_,
         std::vector<RandomizerArea> areas_ = {}, std::vector<TrialKey> trials_ = {});
    Hint(RandomizerHint ownKey_, HintType hintType_, std::vector<RandomizerHintTextKey> hintKeys_,
         std::vector<RandomizerCheck> locations_ = {}, std::vector<RandomizerArea> areas_ = {},
         std::vector<TrialKey> trials_ = {}, bool yourPocket_ = false, int num_ = 0);
    Hint(RandomizerHint ownKey_, std::vector<CustomMessage> messages_);
    Hint(RandomizerHint ownKey_, nlohmann::json json_);
    void FillGapsInData();
    void SetLocationsAsHinted() const;
    void NamesChosen();
    size_t GetNumberOfMessages() const;
    const std::vector<std::string> GetAllMessageStrings(MessageFormat format = MF_AUTO_FORMAT) const;
    const CustomMessage GetHintMessage(MessageFormat format = MF_AUTO_FORMAT, size_t id = 0) const;
    const HintText GetHintText(size_t id = 0) const;
    oJson toJSON();
    void logHint(oJson& jsonData);
    const HintText GetItemHintText(uint8_t slot, bool mysterious = false) const;
    const HintText GetAreaHintText(uint8_t slot) const;
    const CustomMessage GetItemName(uint8_t slot, bool mysterious = false) const;
    const CustomMessage GetAreaName(uint8_t slot) const;
    static CustomMessage GetBridgeReqsText();
    static CustomMessage GetGanonBossKeyText();
    static CustomMessage GetGanonsSoulText();
    static CustomMessage GetWinconText();
    void AddHintedLocation(RandomizerCheck location);
    std::vector<RandomizerCheck> GetHintedLocations() const;
    void SetHintType(HintType type);
    HintType GetHintType() const;
    void AddHintedArea(RandomizerArea area);
    std::vector<RandomizerArea> GetHintedAreas() const;
    void SetDistribution(std::string distribution);
    const std::string& GetDistribution() const;
    bool IsEnabled() const;
    std::vector<RandomizerHintTextKey> GetHintTextKeys() const;
    std::vector<RandomizerGet> GetHintedItems() const;
    std::vector<uint8_t> GetItemNamesChosen() const;
    std::vector<uint8_t> GetHintTextsChosen() const;
    std::vector<uint8_t> GetAreaTextsChosen() const;
    std::vector<TrialKey> GetHintedTrials() const;
    int GetNum();
    void ResetVariables();

    /**
     * @brief Names an area that lives in the OTHER game (combo rando), per slot.
     *
     * RandomizerArea only enumerates OoT areas, so a reward placed in Majora's Mask has no enum to
     * point at and the hint would fall back to RA_NONE. When the slot has a non-empty override here
     * GetAreaName returns it verbatim ("Great Bay Temple") instead of looking up the enum table.
     * Slots left empty behave exactly as before.
     */
    void SetForeignAreas(std::vector<std::string> foreignAreas_);
    const std::vector<std::string>& GetForeignAreas() const;

  private:
    RandomizerHint ownKey = RH_NONE;
    HintType hintType = HINT_TYPE_HINT_KEY;
    std::string distribution = "";
    std::vector<RandomizerHintTextKey> hintKeys = {};
    std::vector<RandomizerCheck> locations = {};
    std::vector<RandomizerArea> areas = {};
    std::vector<TrialKey> trials = {};
    bool yourPocket = false;
    int num = 0;
    std::vector<CustomMessage> messages = {};
    std::vector<RandomizerGet> items = {};
    bool enabled = false;
    std::vector<uint8_t> itemNamesChosen = {};
    std::vector<uint8_t> hintTextsChosen = {};
    std::vector<uint8_t> areaNamesChosen = {};
    // Nombre de área del otro juego por slot; vacío = usar el enum `areas`. Ver SetForeignAreas.
    std::vector<std::string> foreignAreas = {};
};
} // namespace Rando