#include "ModMessages.h"

#include <string>
#include <unordered_map>

#include <spdlog/spdlog.h>

#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/custom-message/CustomMessageManager.h"

namespace {

constexpr uint16_t FirstModMessageId = 0xA000;
constexpr uint16_t LastModMessageId = 0xAFFF;
constexpr uint8_t FirstModNaviEnemyId = 0x5D;
constexpr uint8_t LastModNaviEnemyId = 0xFE;
constexpr uint8_t NoNaviEnemyId = 0xFF;
constexpr uint16_t NaviHintTextBase = 0x0600;

struct ModMessage {
    std::string english;
    std::string german;
    std::string french;
    TextBoxType type;
    TextBoxPosition position;
    bool autoFormat;
};

std::unordered_map<uint16_t, ModMessage> sMessages;
std::unordered_map<std::string, uint16_t> sMessageIdsByKey;
std::unordered_map<std::string, uint8_t> sNaviEnemyIdsByActor;
uint16_t sNextMessageId = FirstModMessageId;
uint8_t sNextNaviEnemyId = FirstModNaviEnemyId;

void LoadMessage(uint16_t* textId, bool* loadFromMessageTable) {
    auto entry = sMessages.find(*textId);
    if (entry == sMessages.end()) {
        return;
    }
    const ModMessage& stored = entry->second;
    CustomMessage message(stored.english, stored.german, stored.french, stored.type, stored.position);

    if (stored.autoFormat) {
        message.AutoFormat();
    } else {
        message.Format();
    }
    message.LoadIntoFont();
    *loadFromMessageTable = false;
}

void StoreMessage(uint16_t textId, ModMessage message) {
    sMessages.emplace(textId, std::move(message));
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnOpenText>(textId, LoadMessage);
}

std::string TranslationOrEnglish(const char* translation, const char* english) {
    return translation != nullptr ? translation : english;
}

bool IsValidMessage(const SOHModMessage* message) {
    return message != nullptr && message->structSize >= sizeof(SOHModMessage) && message->key != nullptr &&
           message->text != nullptr;
}

} // namespace

uint16_t ModMessages_GetId(const char* key) {
    if (key == nullptr) {
        return SOH_MOD_MESSAGE_INVALID;
    }
    auto entry = sMessageIdsByKey.find(key);
    return entry == sMessageIdsByKey.end() ? SOH_MOD_MESSAGE_INVALID : entry->second;
}

uint16_t ModMessages_Register(const SOHModMessage* message) {
    if (!IsValidMessage(message)) {
        return SOH_MOD_MESSAGE_INVALID;
    }
    uint16_t existing = ModMessages_GetId(message->key);

    if (existing != SOH_MOD_MESSAGE_INVALID) {
        return existing;
    }
    if (sNextMessageId > LastModMessageId) {
        SPDLOG_ERROR("[ModMessages] No text ids left for '{}'", message->key);
        return SOH_MOD_MESSAGE_INVALID;
    }
    uint16_t textId = sNextMessageId++;

    StoreMessage(textId, { message->text, TranslationOrEnglish(message->textGerman, message->text),
                           TranslationOrEnglish(message->textFrench, message->text), (TextBoxType)message->textboxType,
                           (TextBoxPosition)message->textboxPosition, message->autoFormat });
    sMessageIdsByKey.emplace(message->key, textId);
    return textId;
}

uint8_t ModMessages_RegisterNaviHint(const char* actorKey, const char* hint) {
    if (actorKey == nullptr || hint == nullptr) {
        return NoNaviEnemyId;
    }
    auto existing = sNaviEnemyIdsByActor.find(actorKey);

    if (existing != sNaviEnemyIdsByActor.end()) {
        return existing->second;
    }
    if (sNextNaviEnemyId > LastModNaviEnemyId) {
        SPDLOG_ERROR("[ModMessages] No Navi hint ids left for '{}'", actorKey);
        return NoNaviEnemyId;
    }
    uint8_t naviEnemyId = sNextNaviEnemyId++;

    StoreMessage(NaviHintTextBase + naviEnemyId, { hint, hint, hint, TEXTBOX_TYPE_BLUE, TEXTBOX_POS_VARIABLE, true });
    sNaviEnemyIdsByActor.emplace(actorKey, naviEnemyId);
    return naviEnemyId;
}
