#include <string.h>
#include "tests/test_require.h"
#include "src/overlays/actors/ovl_En_Viewer/z_en_viewer.h"
#include "src/overlays/actors/ovl_En_Viewer/static_story_actor.h"
#include "variables.h"

void EnViewerStatic_OfferTalk(EnViewer* viewer, PlayState* play);
SaveContext gSaveContext;
u32 gBitFlags[32];
static unsigned processCalls, offerCalls, stateCalls, advanceCalls, closeCalls, flagCalls;
static bool acceptTalk;
static u8 messageState = TEXT_STATE_NONE;
static f32 offeredDistance;
u32 Actor_ProcessTalkRequest(Actor* actor, PlayState* play) {
    (void)actor;
    (void)play;
    processCalls++;
    return acceptTalk;
}
s32 Actor_OfferTalk(Actor* actor, PlayState* play, f32 distance) {
    (void)actor;
    (void)play;
    offeredDistance = distance;
    offerCalls++;
    return 0;
}
u8 Message_GetState(MessageContext* msgCtx) {
    (void)msgCtx;
    stateCalls++;
    return messageState;
}
u8 Message_ShouldAdvance(PlayState* play) {
    (void)play;
    advanceCalls++;
    return 0;
}
void Message_CloseTextbox(PlayState* play) {
    (void)play;
    closeCalls++;
}
s32 Flags_GetEventChkInf(s32 flag) {
    (void)flag;
    flagCalls++;
    return 0;
}
int main(void) {
    EnViewer viewer;
    PlayState play;
    memset(&viewer, 0, sizeof(viewer));
    memset(&play, 0, sizeof(play));
    viewer.staticState.type = STATIC_STORY_ACTOR_PHANTOM_GANON;
    viewer.staticState.talking = true;
    viewer.staticState.tracking = true;
    viewer.staticState.interactInfo.talkState = NPC_TALK_STATE_TALKING;
    viewer.actor.textId = 0x7777;
    EnViewerStatic_OfferTalk(&viewer, &play);
    REQUIRE(viewer.actor.textId == 0);
    REQUIRE(!viewer.staticState.talking);
    REQUIRE(!viewer.staticState.tracking);
    REQUIRE(viewer.staticState.interactInfo.talkState == NPC_TALK_STATE_IDLE);
    REQUIRE(processCalls == 0 && offerCalls == 0 && stateCalls == 0 && advanceCalls == 0 && closeCalls == 0);
    REQUIRE(flagCalls == 0);

    memset(&viewer, 0, sizeof(viewer));
    viewer.staticState.type = STATIC_STORY_ACTOR_ANJU;
    viewer.actor.world.pos = (Vec3f){ 10, 123, 30 };
    viewer.actor.shape.rot.y = 12000;
    AnimationHeader seatedAnimation = { 0 };
    viewer.skin.skelAnime.animation = &seatedAnimation;
    EnViewerStatic_OfferTalk(&viewer, &play);
    REQUIRE(viewer.actor.textId == 0x8F26 && offeredDistance == 90.0f);
    REQUIRE(!viewer.staticState.talking);
    acceptTalk = true;
    EnViewerStatic_OfferTalk(&viewer, &play);
    REQUIRE(viewer.staticState.talking && viewer.staticState.interactInfo.talkState == NPC_TALK_STATE_TALKING);
    messageState = TEXT_STATE_CLOSING;
    EnViewerStatic_OfferTalk(&viewer, &play);
    REQUIRE(!viewer.staticState.talking && viewer.staticState.interactInfo.talkState == NPC_TALK_STATE_IDLE);
    REQUIRE(viewer.skin.skelAnime.animation == &seatedAnimation && viewer.staticState.pose == 0);
    REQUIRE(viewer.actor.world.pos.x == 10 && viewer.actor.world.pos.y == 123 && viewer.actor.world.pos.z == 30);
    REQUIRE(viewer.actor.shape.rot.y == 12000);
    return 0;
}
