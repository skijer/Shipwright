#ifndef STATIC_STORY_TALK_H
#define STATIC_STORY_TALK_H

#include <stdbool.h>
#include <stdint.h>

#include "static_story_actor.h"
#include "static_story_dialogue.h"

typedef enum {
    STATIC_STORY_TALK_MESSAGE_OTHER,
    STATIC_STORY_TALK_MESSAGE_EVENT,
    STATIC_STORY_TALK_MESSAGE_CLOSING,
} StaticStoryTalkMessageState;

typedef struct {
    bool talking;
    bool tracking;
    uint16_t textId;
} StaticStoryTalkSession;

typedef struct {
    StaticStoryTalkMessageState (*getMessageState)(void* context);
    bool (*shouldAdvance)(void* context);
    void (*closeTextbox)(void* context);
    bool (*processTalkRequest)(void* context);
    bool (*offerTalk)(float distance, void* context);
} StaticStoryTalkOperations;

void StaticStoryTalk_Update(StaticStoryActorType type, const StaticStoryProgression* progression, float talkDistance,
                            bool timePedestalOffered, StaticStoryTalkSession* session,
                            const StaticStoryTalkOperations* operations, void* context);

#endif
