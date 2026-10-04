#include <stdbool.h>
#include <stdint.h>

#include "test_require.h"
#include "../src/overlays/actors/ovl_En_Viewer/static_story_talk.h"

typedef struct {
    StaticStoryTalkMessageState messageState;
    bool advance;
    bool process;
    bool offer;
    unsigned getStateCalls;
    unsigned advanceCalls;
    unsigned closeCalls;
    unsigned processCalls;
    unsigned offerCalls;
    float offeredDistance;
} TalkFixture;

static StaticStoryTalkMessageState FixtureGetState(void* context) {
    TalkFixture* fixture = context;
    fixture->getStateCalls++;
    return fixture->messageState;
}

static bool FixtureShouldAdvance(void* context) {
    TalkFixture* fixture = context;
    fixture->advanceCalls++;
    return fixture->advance;
}

static void FixtureClose(void* context) {
    ((TalkFixture*)context)->closeCalls++;
}

static bool FixtureProcess(void* context) {
    TalkFixture* fixture = context;
    fixture->processCalls++;
    return fixture->process;
}

static bool FixtureOffer(float distance, void* context) {
    TalkFixture* fixture = context;
    fixture->offerCalls++;
    fixture->offeredDistance = distance;
    return fixture->offer;
}

static const StaticStoryTalkOperations sOperations = {
    FixtureGetState, FixtureShouldAdvance, FixtureClose, FixtureProcess, FixtureOffer,
};

int main(void) {
    StaticStoryProgression progression = { 0 };
    StaticStoryTalkSession session = { .talking = true, .tracking = true, .textId = 0x7777 };
    TalkFixture fixture = {
        .messageState = STATIC_STORY_TALK_MESSAGE_EVENT,
        .advance = true,
        .process = true,
        .offer = true,
    };

    /* Phantom is rejected before both the active-conversation and offer/request branches. */
    StaticStoryTalk_Update(STATIC_STORY_ACTOR_PHANTOM_GANON, &progression, 120.0f, false, &session, &sOperations,
                           &fixture);
    REQUIRE(!session.talking);
    REQUIRE(!session.tracking);
    REQUIRE(session.textId == 0);
    REQUIRE(fixture.getStateCalls == 0);
    REQUIRE(fixture.advanceCalls == 0);
    REQUIRE(fixture.closeCalls == 0);
    REQUIRE(fixture.processCalls == 0);
    REQUIRE(fixture.offerCalls == 0);

    session = (StaticStoryTalkSession){ 0 };
    fixture = (TalkFixture){ .process = false, .offer = true };
    StaticStoryTalk_Update(STATIC_STORY_ACTOR_SKULL_KID, &progression, 90.0f, false, &session, &sOperations, &fixture);
    REQUIRE(session.textId == STATIC_STORY_TEXT_SKULL_KID);
    REQUIRE(!session.talking);
    REQUIRE(session.tracking);
    REQUIRE(fixture.processCalls == 1);
    REQUIRE(fixture.offerCalls == 1);
    REQUIRE(fixture.offeredDistance == 90.0f);

    session = (StaticStoryTalkSession){ 0 };
    fixture = (TalkFixture){ .process = true, .offer = true };
    /* A request accepted before the pedestal offer remains a real conversation. */
    StaticStoryTalk_Update(STATIC_STORY_ACTOR_SKULL_KID, &progression, 90.0f, true, &session, &sOperations, &fixture);
    REQUIRE(session.talking);
    REQUIRE(session.tracking);
    REQUIRE(fixture.processCalls == 1);
    REQUIRE(fixture.offerCalls == 0);

    fixture = (TalkFixture){ .messageState = STATIC_STORY_TALK_MESSAGE_EVENT, .advance = true };
    StaticStoryTalk_Update(STATIC_STORY_ACTOR_SKULL_KID, &progression, 90.0f, true, &session, &sOperations, &fixture);
    REQUIRE(!session.talking);
    REQUIRE(!session.tracking);
    REQUIRE(fixture.getStateCalls == 1);
    REQUIRE(fixture.advanceCalls == 1);
    REQUIRE(fixture.closeCalls == 1);

    session = (StaticStoryTalkSession){ .talking = true };
    fixture = (TalkFixture){ .messageState = STATIC_STORY_TALK_MESSAGE_CLOSING };
    StaticStoryTalk_Update(STATIC_STORY_ACTOR_SKULL_KID, &progression, 90.0f, false, &session, &sOperations, &fixture);
    REQUIRE(!session.talking);
    REQUIRE(!session.tracking);
    REQUIRE(fixture.closeCalls == 0);

    /* Room-10 Saria and Skull Kid must leave A to the offered time-pedestal interaction. */
    session = (StaticStoryTalkSession){ 0 };
    fixture = (TalkFixture){ .offer = true };
    StaticStoryTalk_Update(STATIC_STORY_ACTOR_SARIA, &progression, 70.0f, true, &session, &sOperations, &fixture);
    REQUIRE(session.textId == 0x1001);
    REQUIRE(!session.talking);
    REQUIRE(!session.tracking);
    REQUIRE(fixture.processCalls == 1);
    REQUIRE(fixture.offerCalls == 0);

    session = (StaticStoryTalkSession){ 0 };
    fixture = (TalkFixture){ .offer = true };
    StaticStoryTalk_Update(STATIC_STORY_ACTOR_SKULL_KID, &progression, 90.0f, true, &session, &sOperations, &fixture);
    REQUIRE(session.textId == STATIC_STORY_TEXT_SKULL_KID);
    REQUIRE(!session.talking);
    REQUIRE(!session.tracking);
    REQUIRE(fixture.processCalls == 1);
    REQUIRE(fixture.offerCalls == 0);

    /* Other catalogue actors and these actors away from a pedestal keep their ordinary talk offers. */
    session = (StaticStoryTalkSession){ 0 };
    fixture = (TalkFixture){ .offer = true };
    StaticStoryTalk_Update(STATIC_STORY_ACTOR_IMPA, &progression, 80.0f, true, &session, &sOperations, &fixture);
    REQUIRE(session.tracking);
    REQUIRE(fixture.offerCalls == 1);

    session = (StaticStoryTalkSession){ 0 };
    fixture = (TalkFixture){ .offer = true };
    StaticStoryTalk_Update(STATIC_STORY_ACTOR_SARIA, &progression, 70.0f, false, &session, &sOperations, &fixture);
    REQUIRE(session.tracking);
    REQUIRE(fixture.offerCalls == 1);
    return 0;
}
