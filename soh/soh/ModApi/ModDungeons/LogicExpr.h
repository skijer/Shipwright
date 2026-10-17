#ifndef SOH_MOD_API_LOGIC_EXPR_H
#define SOH_MOD_API_LOGIC_EXPR_H

#include <cstdint>
#include <string>
#include <vector>

// Condition language of mod dungeons and bosses, compiled once and evaluated against the rando Logic:
//   expr := term ('or' term)*   term := factor ('and' factor)*   factor := 'not' factor | '(' expr ')' | atom
class LogicExpr {
  public:
    // Resolves `event(name)` and `keys(n)` / `bosskey` inside the dungeon that owns the expression.
    struct Scope {
        int32_t dungeon = -1;
    };

    bool Compile(const std::string& text, const Scope& scope, std::string& error);
    bool Evaluate() const;
    bool IsEmpty() const;

  private:
    enum class Op : uint8_t {
        True,
        False,
        And,
        Or,
        Not,
        CanUse,
        HasItem,
        Capability,
        VanillaEvent,
        DungeonEvent,
        Trick,
        Setting,
        CanKill,
        Child,
        Adult,
        Day,
        Night,
        SmallKeys,
        BossKey,
        Tokens,
    };

    struct Node {
        Op op;
        int32_t value = 0;
        int32_t argument = 0;
    };

    std::vector<Node> mProgram;
    Scope mScope;

    friend class LogicExprParser;
};

#endif
