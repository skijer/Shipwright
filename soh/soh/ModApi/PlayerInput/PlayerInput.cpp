#include "PlayerInput.h"

#include <map>
#include <string>

#include "soh/Enhancements/game-interactor/GameInteractor.h"

extern "C" {
#include "macros.h"
#include "z64.h"
}

namespace {

struct InputBlock {
    uint16_t buttons;
    bool stick;
};

std::map<std::string, InputBlock> sBlocks;
uint16_t sBlockedButtons = 0;
bool sStickBlocked = false;

void RecomputeBlocks() {
    sBlockedButtons = 0;
    sStickBlocked = false;
    for (const auto& [owner, block] : sBlocks) {
        sBlockedButtons |= block.buttons;
        sStickBlocked = sStickBlocked || block.stick;
    }
}

void ClearPad(OSContPad& pad) {
    pad.button &= ~sBlockedButtons;
    if (sStickBlocked) {
        pad.stick_x = 0;
        pad.stick_y = 0;
    }
}

void FilterPlayerInput(Player* player, Input* input) {
    if (sBlockedButtons == 0 && !sStickBlocked) {
        return;
    }
    ClearPad(input->cur);
    ClearPad(input->prev);
    ClearPad(input->press);
    ClearPad(input->rel);
}

} // namespace

void PlayerInput_Init() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayerFilterInput>(FilterPlayerInput);
}

bool PlayerInput_Block(const char* owner, uint16_t buttons, bool blockStick) {
    if (owner == nullptr || owner[0] == '\0') {
        return false;
    }
    sBlocks[owner] = { buttons, blockStick };
    RecomputeBlocks();
    return true;
}

void PlayerInput_Release(const char* owner) {
    if (owner == nullptr || sBlocks.erase(owner) == 0) {
        return;
    }
    RecomputeBlocks();
}

uint16_t PlayerInput_GetBlockedButtons(void) {
    return sBlockedButtons;
}

bool PlayerInput_IsStickBlocked(void) {
    return sStickBlocked;
}
