#include "soh/ModApi/ModDungeons/LogicExpr.h"

#include <cctype>
#include <optional>

#include <spdlog/spdlog.h>

#include "soh/ModApi/ModDungeons/DungeonRegistry.h"
#include "soh/ModApi/RandoItems/RandoItems.h"
#include "soh/ModApi/RandoLogic/RandoLogic.h"
#include "soh/ModApi/ModDungeons/RandoEnumNames.h"
#include "soh/Enhancements/randomizer/location_access.h"

namespace {

const char* const kCapabilityNames[RLC_MAX] = {
#define LOGIC_EXPR_CAPABILITY_NAME(name) #name,
    RANDO_LOGIC_CAPABILITIES(LOGIC_EXPR_CAPABILITY_NAME)
#undef LOGIC_EXPR_CAPABILITY_NAME
};

int32_t FindCapability(const std::string& name) {
    for (int32_t index = 0; index < RLC_MAX; index++) {
        if (name == kCapabilityNames[index]) {
            return index;
        }
    }
    return -1;
}

bool IsNameCharacter(char character) {
    return std::isalnum(static_cast<unsigned char>(character)) || character == '_' || character == '.' ||
           character == ':' || character == '-';
}

} // namespace

class LogicExprParser {
  public:
    LogicExprParser(const std::string& text, LogicExpr& output) : mText(text), mOutput(output) {
    }

    bool Parse(std::string& error) {
        bool parsed = ParseOr();
        SkipSpaces();
        if (parsed && mPosition != mText.size()) {
            Fail("unexpected '" + mText.substr(mPosition, 1) + "'");
            parsed = false;
        }
        error = mError;
        return parsed && mError.empty();
    }

  private:
    using Op = LogicExpr::Op;

    void Fail(const std::string& message) {
        if (mError.empty()) {
            mError = message + " at column " + std::to_string(mPosition + 1);
        }
    }

    void Emit(Op op, int32_t value = 0, int32_t argument = 0) {
        mOutput.mProgram.push_back({ op, value, argument });
    }

    void SkipSpaces() {
        while (mPosition < mText.size() && std::isspace(static_cast<unsigned char>(mText[mPosition]))) {
            mPosition++;
        }
    }

    bool Accept(const std::string& symbol) {
        SkipSpaces();
        if (mText.compare(mPosition, symbol.size(), symbol) != 0) {
            return false;
        }
        mPosition += symbol.size();
        return true;
    }

    bool AcceptWord(const std::string& word) {
        SkipSpaces();
        size_t end = mPosition + word.size();
        if (mText.compare(mPosition, word.size(), word) != 0 || (end < mText.size() && IsNameCharacter(mText[end]))) {
            return false;
        }
        mPosition = end;
        return true;
    }

    std::string ReadName() {
        SkipSpaces();
        size_t start = mPosition;
        while (mPosition < mText.size() && IsNameCharacter(mText[mPosition])) {
            mPosition++;
        }
        return mText.substr(start, mPosition - start);
    }

    std::optional<int32_t> ReadNumber() {
        std::string digits = ReadName();
        if (digits.empty() || !std::isdigit(static_cast<unsigned char>(digits[0]))) {
            Fail("expected a number");
            return std::nullopt;
        }
        return static_cast<int32_t>(std::stol(digits, nullptr, 0));
    }

    bool Expect(const std::string& symbol) {
        if (!Accept(symbol)) {
            Fail("expected '" + symbol + "'");
            return false;
        }
        return true;
    }

    bool ParseOr() {
        if (!ParseAnd()) {
            return false;
        }
        while (AcceptWord("or") || Accept("||")) {
            if (!ParseAnd()) {
                return false;
            }
            Emit(Op::Or);
        }
        return true;
    }

    bool ParseAnd() {
        if (!ParseNot()) {
            return false;
        }
        while (AcceptWord("and") || Accept("&&")) {
            if (!ParseNot()) {
                return false;
            }
            Emit(Op::And);
        }
        return true;
    }

    bool ParseNot() {
        if (AcceptWord("not") || Accept("!")) {
            if (!ParseNot()) {
                return false;
            }
            Emit(Op::Not);
            return true;
        }
        if (Accept("(")) {
            return ParseOr() && Expect(")");
        }
        return ParseAtom();
    }

    template <typename T> std::optional<int32_t> ReadEnum(const char* what) {
        std::string name = ReadName();
        std::optional<T> value = RandoEnumFromName<T>(name);
        if (!value.has_value()) {
            Fail(std::string("unknown ") + what + " '" + name + "'");
            return std::nullopt;
        }
        return static_cast<int32_t>(*value);
    }

    std::optional<int32_t> ReadItem() {
        std::string name = ReadName();
        if (std::optional<RandomizerGet> vanilla = RandoEnumFromName<RandomizerGet>(name)) {
            return static_cast<int32_t>(*vanilla);
        }
        int32_t modItem = RandoItems_GetRandoGet(name.c_str());
        if (modItem == 0) {
            Fail("unknown item '" + name + "'");
            return std::nullopt;
        }
        return modItem;
    }

    bool EmitCall(Op op, std::optional<int32_t> value, int32_t argument = 0) {
        if (!value.has_value() || !Expect(")")) {
            return false;
        }
        Emit(op, *value, argument);
        return true;
    }

    bool ParseCall(const std::string& name) {
        if (name == "has") {
            return EmitCall(Op::HasItem, ReadItem());
        }
        if (name == "trick") {
            return EmitCall(Op::Trick, ReadEnum<RandomizerTrick>("trick"));
        }
        if (name == "canKill") {
            return EmitCall(Op::CanKill, ReadEnum<RandomizerEnemy>("enemy"));
        }
        if (name == "keys") {
            return EmitCall(Op::SmallKeys, ReadNumber());
        }
        if (name == "tokens") {
            return EmitCall(Op::Tokens, ReadNumber());
        }
        if (name == "setting") {
            std::optional<int32_t> setting = ReadEnum<RandomizerSettingKey>("setting");
            if (!setting.has_value() || !Expect(",")) {
                return false;
            }
            std::optional<int32_t> value = ReadNumber();
            return value.has_value() && EmitCall(Op::Setting, setting, *value);
        }
        if (name == "event") {
            return ParseEvent() && Expect(")");
        }
        Fail("unknown function '" + name + "'");
        return false;
    }

    // Event names are region-style labels, so they may hold spaces: everything up to the closing parenthesis.
    std::string ReadEventName() {
        SkipSpaces();
        size_t end = mText.find(')', mPosition);
        if (end == std::string::npos) {
            end = mText.size();
        }
        std::string name = mText.substr(mPosition, end - mPosition);
        mPosition = end;
        while (!name.empty() && std::isspace(static_cast<unsigned char>(name.back()))) {
            name.pop_back();
        }
        return name;
    }

    bool ParseEvent() {
        std::string name = ReadEventName();
        int32_t dungeonEvent = DungeonRegistry_FindEvent(mOutput.mScope.dungeon, name);
        if (dungeonEvent >= 0) {
            Emit(Op::DungeonEvent, dungeonEvent);
            return true;
        }
        if (std::optional<LogicVal> vanilla = RandoEnumFromName<LogicVal>(name)) {
            Emit(Op::VanillaEvent, *vanilla);
            return true;
        }
        Fail("unknown event '" + name + "'");
        return false;
    }

    bool ParseAtom() {
        size_t start = mPosition;
        std::string name = ReadName();
        if (name.empty()) {
            Fail("expected a condition");
            return false;
        }
        if (Accept("(")) {
            return ParseCall(name);
        }
        static const std::pair<const char*, Op> kWords[] = {
            { "true", Op::True },   { "false", Op::False }, { "child", Op::Child },     { "adult", Op::Adult },
            { "day", Op::Day },     { "night", Op::Night }, { "bosskey", Op::BossKey },
        };
        for (const auto& [word, op] : kWords) {
            if (name == word) {
                Emit(op);
                return true;
            }
        }
        int32_t capability = FindCapability(name);
        if (capability >= 0) {
            Emit(Op::Capability, capability);
            return true;
        }
        mPosition = start;
        std::optional<int32_t> item = ReadItem();
        if (item.has_value()) {
            Emit(Op::CanUse, *item);
            return true;
        }
        return false;
    }

    const std::string& mText;
    LogicExpr& mOutput;
    size_t mPosition = 0;
    std::string mError;
};

bool LogicExpr::Compile(const std::string& text, const Scope& scope, std::string& error) {
    mProgram.clear();
    mScope = scope;
    if (text.empty()) {
        return true;
    }
    LogicExprParser parser(text, *this);
    if (!parser.Parse(error)) {
        mProgram.clear();
        return false;
    }
    return true;
}

bool LogicExpr::IsEmpty() const {
    return mProgram.empty();
}

bool LogicExpr::Evaluate() const {
    if (mProgram.empty()) {
        return true;
    }
    std::vector<bool> stack;
    stack.reserve(mProgram.size());
    for (const Node& node : mProgram) {
        switch (node.op) {
            case Op::And:
            case Op::Or: {
                bool right = stack.back();
                stack.pop_back();
                stack.back() = node.op == Op::And ? (stack.back() && right) : (stack.back() || right);
                break;
            }
            case Op::Not:
                stack.back() = !stack.back();
                break;
            case Op::True:
                stack.push_back(true);
                break;
            case Op::False:
                stack.push_back(false);
                break;
            case Op::CanUse:
                stack.push_back(logic->CanUse(static_cast<RandomizerGet>(node.value)));
                break;
            case Op::HasItem:
                stack.push_back(logic->HasItem(static_cast<RandomizerGet>(node.value)));
                break;
            case Op::Capability:
                stack.push_back(RandoLogic_HasCapability(kCapabilityNames[node.value]));
                break;
            case Op::VanillaEvent:
            case Op::DungeonEvent:
                stack.push_back(logic->Get(static_cast<LogicVal>(node.value)));
                break;
            case Op::Trick:
                stack.push_back(ctx->GetTrickOption(static_cast<RandomizerTrick>(node.value)).Get() != 0);
                break;
            case Op::Setting:
                stack.push_back(ctx->GetOption(static_cast<RandomizerSettingKey>(node.value)).Is(node.argument));
                break;
            case Op::CanKill:
                stack.push_back(logic->CanKillEnemy(static_cast<RandomizerEnemy>(node.value)));
                break;
            case Op::Child:
                stack.push_back(logic->IsChild);
                break;
            case Op::Adult:
                stack.push_back(logic->IsAdult);
                break;
            case Op::Day:
                stack.push_back(logic->AtDay);
                break;
            case Op::Night:
                stack.push_back(logic->AtNight);
                break;
            case Op::SmallKeys:
                stack.push_back(DungeonRegistry_HasSmallKeys(mScope.dungeon, node.value));
                break;
            case Op::BossKey:
                stack.push_back(DungeonRegistry_HasBossKey(mScope.dungeon));
                break;
            case Op::Tokens:
                stack.push_back(logic->GetGSCount() >= node.value);
                break;
            default:
                SPDLOG_ERROR("[LogicExpr] unknown op {}", static_cast<int>(node.op));
                return false;
        }
    }
    return stack.size() == 1 && stack.back();
}
