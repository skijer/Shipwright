#include <soh/OTRGlobals.h>
#include "soh/Enhancements/custom-message/CustomMessageManager.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"

extern "C" {
#include "src/overlays/actors/ovl_En_Viewer/static_story_dialogue.h"
#include "variables.h"
}

static bool LoadStaticStoryDialogueIntoFont(const char* text, uint8_t language, void* context) {
    (void)language;
    (void)context;
    CustomMessage message(text, text, text);

    message.AutoFormat();
    message.LoadIntoFont();
    return true;
}

void StaticStoryDialogue_OnOpenText(uint16_t* textId, bool* loadFromMessageTable) {
    if (textId == nullptr) {
        return;
    }
    StaticStoryDialogue_HandleOpenText(*textId, gSaveContext.language, loadFromMessageTable,
                                       LoadStaticStoryDialogueIntoFont, nullptr);
}

void StaticStoryDialogue_Register() {
    COND_ID_HOOK(OnOpenText, STATIC_STORY_TEXT_TREASURE_CHEST_SHOP_GAL, true, StaticStoryDialogue_OnOpenText);
    COND_ID_HOOK(OnOpenText, STATIC_STORY_TEXT_SKULL_KID, true, StaticStoryDialogue_OnOpenText);
    COND_ID_HOOK(OnOpenText, STATIC_STORY_TEXT_KEATON, true, StaticStoryDialogue_OnOpenText);
    COND_ID_HOOK(OnOpenText, STATIC_STORY_TEXT_HAPPY_MASK_SALESMAN, true, StaticStoryDialogue_OnOpenText);
    COND_ID_HOOK(OnOpenText, STATIC_STORY_TEXT_CHILD_KAFEI, true, StaticStoryDialogue_OnOpenText);
    COND_ID_HOOK(OnOpenText, STATIC_STORY_TEXT_LULU, true, StaticStoryDialogue_OnOpenText);
    COND_ID_HOOK(OnOpenText, STATIC_STORY_TEXT_ANJU, true, StaticStoryDialogue_OnOpenText);
}

static RegisterShipInitFunc initFunc(StaticStoryDialogue_Register);
