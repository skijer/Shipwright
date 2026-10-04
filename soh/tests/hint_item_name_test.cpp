#include <array>
#include <iostream>
#include <map>
#include "soh/Enhancements/randomizer/item.h"
#include "tests/test_require.h"

// Seed storage is the external boundary. Item and localized message types are real.
namespace Rando {
struct StaticData {
    static inline std::map<RandomizerHintTextKey, HintText> hintTextTable;
};
struct FixtureLocation {
    Item item;
    RandomizerGet GetPlacedRandomizerGet() const {
        return item.GetRandomizerGet();
    }
    const Item& GetPlacedItem() const {
        return item;
    }
};
struct FixtureOption {
    uint8_t value = RO_HINT_CLARITY_CLEAR;
    bool Is(uint8_t other) const {
        return value == other;
    }
};
struct FixtureTrap {
    std::string GetTrickName() const {
        return "Hookshat";
    }
    std::string GetTrickArticle() const {
        return "a ";
    }
};
struct Context {
    std::array<FixtureLocation, 2> locations;
    std::map<RandomizerCheck, FixtureTrap> overrides;
    FixtureOption clarity;
    static Context* GetInstance() {
        static Context ctx;
        return &ctx;
    }
    FixtureLocation* GetItemLocation(RandomizerCheck check) {
        return &locations.at(static_cast<size_t>(check));
    }
    FixtureOption GetOption(RandomizerSettingKey) const {
        return clarity;
    }
};
struct Hint {
    std::vector<RandomizerCheck> locations{ static_cast<RandomizerCheck>(0), static_cast<RandomizerCheck>(1) };
    std::vector<uint8_t> itemNamesChosen;
    const HintText GetItemHintText(uint8_t, bool = false) const;
    const CustomMessage GetItemName(uint8_t, bool = false) const;
};
} // namespace Rando

#ifdef COMBO_BUILD
namespace ComboRando {
struct ForeignItem {
    std::string displayName, fakeTrickName;
};
} // namespace ComboRando
static const ComboRando::ForeignItem* fixtureForeign;
const ComboRando::ForeignItem* OOT_LookupForeignByCheck(RandomizerCheck) {
    return fixtureForeign;
}
#endif

// All assertions use raw text. Screen layout/control-code processing is outside this regression.
void CustomMessage::ProcessMessageFormat(std::string&, MessageFormat format) const {
    REQUIRE(format == MF_RAW);
}
#include "hint_item_name_production.inc"

using namespace Rando;
static Item MakeItem(RandomizerGet rg, Text name, RandomizerHintTextKey key = RHT_NONE, Text article = {},
                     ItemType type = ITEMTYPE_ITEM) {
    return Item(rg, name, type, 0, true, LOGIC_NONE, key, ITEM_CATEGORY_MAJOR, article);
}

int main() {
    auto ctx = Context::GetInstance();
    Hint hint;
    StaticData::hintTextTable[RHT_NONE] = HintText(CustomMessage("No Hint", "Kein Hinweis", "Pas d'Indice"));
    StaticData::hintTextTable[RHT_MYSTERIOUS_ITEM] = HintText(CustomMessage("something"));
    StaticData::hintTextTable[RHT_PROGRESSIVE_HOOKSHOT] =
        HintText(CustomMessage("a Hookshot"), { CustomMessage("a tool") }, { CustomMessage("a grappling device") });

    const std::map<RandomizerGet, Item> rows = {
#include "hint_item_name_rows.inc"
    };
    ctx->locations[0].item = rows.at(RG_SPINNER);
    auto sentence = CustomMessage("They say that #[[1]]# can be found at #[[2]]#.");
    sentence.InsertNames({ hint.GetItemName(0), CustomMessage("the Market") });
    REQUIRE(sentence.GetEnglish(MF_RAW) == "They say that #Spinner# can be found at #the Market#.");

    int checked = 0;
    for (uint8_t mode : { RO_HINT_CLARITY_CLEAR, RO_HINT_CLARITY_AMBIGUOUS, RO_HINT_CLARITY_OBSCURE }) {
        ctx->clarity.value = mode;
        for (const auto& [rg, item] : rows) {
            if (rg == RG_NONE || rg == RG_HINT || rg == RG_SOLD_OUT)
                continue;
#ifdef COMBO_BUILD
            if (rg == RG_COMBO_FOREIGN)
                continue;
#endif
            ctx->locations[0].item = item;
            const auto result = hint.GetItemName(0);
            const Text expected = item.GetArticle() + item.GetName();
            REQUIRE(result.GetEnglish(MF_RAW) == expected.GetEnglish());
            REQUIRE(result.GetFrench(MF_RAW) ==
                    (expected.GetFrench().starts_with(TODO_TRANSLATE) ? expected.GetEnglish() : expected.GetFrench()));
            REQUIRE(result.GetGerman(MF_RAW) ==
                    (expected.GetGerman().starts_with(TODO_TRANSLATE) ? expected.GetEnglish() : expected.GetGerman()));
            REQUIRE(result.GetEnglish(MF_RAW) != "No Hint");
            REQUIRE(hint.GetItemName(0, true).GetEnglish(MF_RAW) == "something");
            ++checked;
        }
        ctx->locations[0].item = MakeItem(RG_PROGRESSIVE_HOOKSHOT, Text("Hookshot"), RHT_PROGRESSIVE_HOOKSHOT);
        const std::map<uint8_t, std::string> clues{ { RO_HINT_CLARITY_CLEAR, "a Hookshot" },
                                                    { RO_HINT_CLARITY_AMBIGUOUS, "a tool" },
                                                    { RO_HINT_CLARITY_OBSCURE, "a grappling device" } };
        REQUIRE(hint.GetItemName(0).GetEnglish(MF_RAW) == clues.at(mode));
    }
    // Independent literals catch language order, articles, empty translations and shared scratch storage.
    ctx->locations[0].item = rows.at(RG_FIRE_ROD);
    ctx->locations[1].item = rows.at(RG_ICE_ROD);
    auto fire = hint.GetItemName(0);
    REQUIRE(hint.GetItemName(1).GetEnglish(MF_RAW) == "Ice Rod");
    REQUIRE(fire.GetEnglish(MF_RAW) == "Fire Rod");
    REQUIRE(fire.GetGerman(MF_RAW) == "Feuerstab");
    REQUIRE(fire.GetFrench(MF_RAW) == "Bâton de Feu");
    ctx->locations[0].item =
        MakeItem(RG_FISH, Text{ "Fish", "Poisson", "Fisch" }, RHT_NONE, Text{ "a ", "un ", "einen " });
    REQUIRE(hint.GetItemName(0).GetEnglish(MF_RAW) == "a Fish");
    REQUIRE(hint.GetItemName(0).GetFrench(MF_RAW) == "un Poisson");
    REQUIRE(hint.GetItemName(0).GetGerman(MF_RAW) == "einen Fisch");
    ctx->locations[0].item = MakeItem(RG_SPINNER, Text{ "Spinner", "", "" });
    REQUIRE(hint.GetItemName(0).GetFrench(MF_RAW) == "Spinner");
    REQUIRE(hint.GetItemName(0).GetGerman(MF_RAW) == "Spinner");
    for (auto rg : { RG_NONE, RG_HINT, RG_SOLD_OUT }) {
        ctx->locations[0].item = rows.at(rg);
        REQUIRE(hint.GetItemName(0).GetEnglish(MF_RAW) == "No Hint");
    }
    ctx->locations[0].item = MakeItem(RG_ICE_TRAP, Text("Ice Trap"));
#ifdef COMBO_BUILD
    REQUIRE(hint.GetItemName(0).GetEnglish(MF_RAW) == "a Hookshat");
    ctx->locations[0].item = MakeItem(RG_COMBO_FOREIGN, Text("Foreign item"));
    ComboRando::ForeignItem foreign{ "MM Bow", "" };
    fixtureForeign = &foreign;
    REQUIRE(hint.GetItemName(0).GetEnglish(MF_RAW) == "MM Bow");
    foreign.fakeTrickName = "MM Baw";
    REQUIRE(hint.GetItemName(0).GetEnglish(MF_RAW) == "MM Baw");
    fixtureForeign = nullptr;
    REQUIRE(hint.GetItemName(0).GetEnglish(MF_RAW) == "something");
#else
    REQUIRE(hint.GetItemName(0).GetEnglish(MF_RAW) == "Hookshat");
#endif
    REQUIRE(StaticData::hintTextTable[RHT_NONE].GetClear().GetEnglish(MF_RAW) == "No Hint");
    std::cout << "PASS " << checked
              << " item/clarity cases in three languages; Market sentence, articles, "
                 "clues, mysterious names, sentinel isolation, slot lifetime and trap behavior\n";
}
