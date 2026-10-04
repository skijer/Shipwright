#include "static_story_talk.h"

#include <stddef.h>

static void StaticStoryTalk_RestorePlacementPose(StaticStoryTalkSession* session) {
    session->talking = false;
    session->tracking = false;
}

void StaticStoryTalk_Update(StaticStoryActorType type, const StaticStoryProgression* progression, float talkDistance,
                            bool timePedestalOffered, StaticStoryTalkSession* session,
                            const StaticStoryTalkOperations* operations, void* context) {
    StaticStoryTalkMessageState messageState;

    if (session == NULL || operations == NULL) {
        return;
    }
    if (!StaticStoryActor_CanTalk(type)) {
        session->textId = 0;
        StaticStoryTalk_RestorePlacementPose(session);
        return;
    }
    if (session->talking) {
        session->tracking = true;
        messageState = operations->getMessageState != NULL ? operations->getMessageState(context)
                                                           : STATIC_STORY_TALK_MESSAGE_OTHER;
        if (messageState == STATIC_STORY_TALK_MESSAGE_EVENT && operations->shouldAdvance != NULL &&
            StaticStoryActor_ShouldCloseEventMessage(true, operations->shouldAdvance(context))) {
            if (operations->closeTextbox != NULL) {
                operations->closeTextbox(context);
            }
            StaticStoryTalk_RestorePlacementPose(session);
        } else if (messageState == STATIC_STORY_TALK_MESSAGE_CLOSING) {
            StaticStoryTalk_RestorePlacementPose(session);
        }
        return;
    }

    session->textId = StaticStoryActor_SelectTextId(type, progression);
    if (operations->processTalkRequest != NULL && operations->processTalkRequest(context)) {
        session->talking = true;
        session->tracking = true;
    } else if (timePedestalOffered && (type == STATIC_STORY_ACTOR_SARIA || type == STATIC_STORY_ACTOR_SKULL_KID)) {
        session->tracking = false;
    } else {
        session->tracking = operations->offerTalk != NULL && operations->offerTalk(talkDistance, context);
    }
}
