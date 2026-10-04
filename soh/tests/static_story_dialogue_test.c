#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "test_require.h"
#include "../src/overlays/actors/ovl_En_Viewer/static_story_dialogue.h"

typedef struct {
    unsigned calls;
    uint8_t language;
    const char* text;
    bool result;
} LoaderFixture;

static bool FixtureLoad(const char* text, uint8_t language, void* context) {
    LoaderFixture* fixture = context;

    fixture->calls++;
    fixture->language = language;
    fixture->text = text;
    return fixture->result;
}

int main(void) {
    static const struct {
        uint16_t id;
        const char* text;
    } expected[] = {
        { STATIC_STORY_TEXT_TREASURE_CHEST_SHOP_GAL, "Finding out what's inside is half the fun!" },
        { STATIC_STORY_TEXT_SKULL_KID, "That won't do you any good. Hee, hee." },
        { STATIC_STORY_TEXT_KEATON, "We Keatons can recognize our own by the sheen of our tails." },
        { STATIC_STORY_TEXT_HAPPY_MASK_SALESMAN, "You've met with a terrible fate, haven't you?" },
        { STATIC_STORY_TEXT_CHILD_KAFEI, "I've made a promise to Anju." },
        { STATIC_STORY_TEXT_LULU, "Pleased to meet you. I'm Lulu." },
        { STATIC_STORY_TEXT_ANJU, "...Kafei... I promised I'd wait for you." },
    };

    for (size_t entry = 0; entry < sizeof(expected) / sizeof(expected[0]); ++entry) {
        for (uint8_t language = 0; language < 4; ++language) {
            const char* text = StaticStoryDialogue_GetText(expected[entry].id, language);

            REQUIRE(text != NULL);
            REQUIRE(strcmp(text, expected[entry].text) == 0);
        }
    }
    REQUIRE(StaticStoryDialogue_GetText(0, 0) == NULL);
    REQUIRE(StaticStoryDialogue_GetText(0xFFFF, 3) == NULL);

    LoaderFixture fixture = { .result = true };
    bool loadFromMessageTable = true;
    REQUIRE(
        StaticStoryDialogue_HandleOpenText(STATIC_STORY_TEXT_LULU, 2, &loadFromMessageTable, FixtureLoad, &fixture));
    REQUIRE(fixture.calls == 1);
    REQUIRE(fixture.language == 2);
    REQUIRE(strcmp(fixture.text, "Pleased to meet you. I'm Lulu.") == 0);
    REQUIRE(!loadFromMessageTable);

    fixture = (LoaderFixture){ .result = true };
    loadFromMessageTable = true;
    REQUIRE(StaticStoryDialogue_HandleOpenText(0x8F26, 3, &loadFromMessageTable, FixtureLoad, &fixture));
    REQUIRE(fixture.calls == 1 && fixture.language == 3);
    REQUIRE(!loadFromMessageTable && strstr(fixture.text, "Kafei") != NULL);

    fixture = (LoaderFixture){ .result = false };
    loadFromMessageTable = true;
    REQUIRE(!StaticStoryDialogue_HandleOpenText(STATIC_STORY_TEXT_SKULL_KID, 1, &loadFromMessageTable, FixtureLoad,
                                                &fixture));
    REQUIRE(fixture.calls == 1);
    REQUIRE(loadFromMessageTable);

    fixture = (LoaderFixture){ .result = true };
    loadFromMessageTable = true;
    REQUIRE(!StaticStoryDialogue_HandleOpenText(0x1234, 3, &loadFromMessageTable, FixtureLoad, &fixture));
    REQUIRE(fixture.calls == 0);
    REQUIRE(loadFromMessageTable);
    REQUIRE(!StaticStoryDialogue_HandleOpenText(STATIC_STORY_TEXT_LULU, 0, NULL, FixtureLoad, &fixture));
    REQUIRE(!StaticStoryDialogue_HandleOpenText(STATIC_STORY_TEXT_LULU, 0, &loadFromMessageTable, NULL, &fixture));
    return 0;
}
