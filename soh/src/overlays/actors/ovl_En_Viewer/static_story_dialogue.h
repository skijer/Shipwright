#ifndef STATIC_STORY_DIALOGUE_H
#define STATIC_STORY_DIALOGUE_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    STATIC_STORY_TEXT_TREASURE_CHEST_SHOP_GAL = 0x8F20,
    STATIC_STORY_TEXT_SKULL_KID = 0x8F21,
    STATIC_STORY_TEXT_KEATON = 0x8F22,
    STATIC_STORY_TEXT_HAPPY_MASK_SALESMAN = 0x8F23,
    STATIC_STORY_TEXT_CHILD_KAFEI = 0x8F24,
    STATIC_STORY_TEXT_LULU = 0x8F25,
    STATIC_STORY_TEXT_ANJU = 0x8F26,
} StaticStoryDialogueTextId;

typedef bool (*StaticStoryDialogueLoadFunc)(const char* text, uint8_t language, void* context);

const char* StaticStoryDialogue_GetText(uint16_t textId, uint8_t language);
bool StaticStoryDialogue_HandleOpenText(uint16_t textId, uint8_t language, bool* loadFromMessageTable,
                                        StaticStoryDialogueLoadFunc load, void* context);

#endif
