#pragma once
#include "model.h"
#include <cmath>

namespace pak {
enum class PieceKind : uint8_t { Sword, Shield, Bow, Hookshot, Hammer, Boomerang, Slingshot, FairyOcarina, Ocarina };

struct HeldItem {
    PieceKind kind;
    int variant;
};

struct Piece {
    std::string key;
    std::string label;
    PieceKind kind;
    int variant = 0;
    Gfx* hilt = nullptr;
    Gfx* blade = nullptr;
    Gfx* sheath = nullptr;
    Gfx* main = nullptr;
    bool backSpace = false;
};

static constexpr uint32_t kLeftHand = 0x5098;
static constexpr uint32_t kLeftFist = 0x50A0;
static constexpr uint32_t kRightHand = 0x50B0;
static constexpr uint32_t kRightFist = 0x50B8;

static std::string PieceName(PieceKind kind, int variant) {
    static const char* const kSwords[] = { "Kokiri Sword", "Master Sword", "Biggoron Sword" };
    static const char* const kShields[] = { "Deku Shield", "Hylian Shield", "Mirror Shield" };
    switch (kind) {
        case PieceKind::Sword:
            return kSwords[variant];
        case PieceKind::Shield:
            return kShields[variant];
        case PieceKind::Bow:
            return "Bow";
        case PieceKind::Hookshot:
            return "Hookshot";
        case PieceKind::Hammer:
            return "Megaton Hammer";
        case PieceKind::Boomerang:
            return "Boomerang";
        case PieceKind::Slingshot:
            return "Slingshot";
        case PieceKind::FairyOcarina:
            return "Fairy Ocarina";
        case PieceKind::Ocarina:
            return "Ocarina of Time";
        default:
            return "Equipment";
    }
}

static std::string SlotKey(PieceKind kind, int variant) {
    std::string name = PieceName(kind, variant);
    name.erase(std::remove(name.begin(), name.end(), ' '), name.end());
    return name;
}

static uint32_t SingleAlias(PieceKind kind, int variant) {
    switch (kind) {
        case PieceKind::Shield:
            return 0x5108 + variant * 8;
        case PieceKind::Bow:
            return 0x5138;
        case PieceKind::Hookshot:
            return 0x5148;
        case PieceKind::Hammer:
            return 0x51F0;
        case PieceKind::Boomerang:
            return 0x5178;
        case PieceKind::Slingshot:
            return 0x5180;
        case PieceKind::FairyOcarina:
            return 0x5190;
        case PieceKind::Ocarina:
            return 0x5128;
        default:
            return 0;
    }
}

static Gfx* FindAlias(const Model& model, uint32_t alias) {
    auto found = model.aliases.find(alias);
    return found == model.aliases.end() ? nullptr : found->second;
}

static bool IsBackShield(const Model& model, int variant) {
    return variant == 1 && model.child && !model.mm && !model.equipmentOnly;
}

static std::vector<Piece> CollectPieces(const Model& model) {
    std::vector<Piece> pieces;
    for (int variant = 0; variant < 3; ++variant) {
        Piece sword{ "", "", PieceKind::Sword, variant };
        sword.hilt = FindAlias(model, 0x50D8 + variant * 8);
        sword.blade = FindAlias(model, 0x50F0 + variant * 8);
        sword.sheath = FindAlias(model, 0x50C0 + variant * 8);
        if (sword.hilt != nullptr || sword.blade != nullptr) {
            pieces.push_back(sword);
        }
    }
    static constexpr PieceKind kSingles[] = { PieceKind::Shield,    PieceKind::Shield,    PieceKind::Shield,
                                              PieceKind::Bow,       PieceKind::Hookshot,  PieceKind::Hammer,
                                              PieceKind::Boomerang, PieceKind::Slingshot, PieceKind::FairyOcarina,
                                              PieceKind::Ocarina };
    int shield = 0;
    for (PieceKind kind : kSingles) {
        int variant = kind == PieceKind::Shield ? shield++ : 0;
        Piece piece{ "", "", kind, variant };
        piece.main = FindAlias(model, SingleAlias(kind, variant));
        piece.backSpace = kind == PieceKind::Shield && IsBackShield(model, variant);
        if (piece.main != nullptr) {
            pieces.push_back(piece);
        }
    }
    return pieces;
}

static bool HasPart(const std::string& name, const char* part) {
    return name.find(part) != std::string::npos;
}

static bool ParseHeldItem(const std::string& name, HeldItem& item) {
    static const std::pair<const char*, HeldItem> kItems[] = {
        { "KokiriSword", { PieceKind::Sword, 0 } },
        { "MasterSword", { PieceKind::Sword, 1 } },
        { "RazorSword", { PieceKind::Sword, 1 } },
        { "Bgs", { PieceKind::Sword, 2 } },
        { "GiantsKnife", { PieceKind::Sword, 2 } },
        { "GildedSword", { PieceKind::Sword, 2 } },
        { "Hammer", { PieceKind::Hammer, 0 } },
        { "Boomerang", { PieceKind::Boomerang, 0 } },
        { "DekuShield", { PieceKind::Shield, 0 } },
        { "HylianShield", { PieceKind::Shield, 1 } },
        { "HerosShield", { PieceKind::Shield, 1 } },
        { "MirrorShield", { PieceKind::Shield, 2 } },
        { "Bow", { PieceKind::Bow, 0 } },
        { "Hookshot", { PieceKind::Hookshot, 0 } },
        { "Slingshot", { PieceKind::Slingshot, 0 } },
        { "FairyOcarina", { PieceKind::FairyOcarina, 0 } },
        { "Oot", { PieceKind::Ocarina, 0 } },
        { "Ocarina", { PieceKind::Ocarina, 0 } },
    };
    for (const auto& [part, held] : kItems) {
        if (HasPart(name, part)) {
            item = held;
            return true;
        }
    }
    return false;
}

static bool IsLeftHandItem(PieceKind kind) {
    return kind == PieceKind::Sword || kind == PieceKind::Hammer || kind == PieceKind::Boomerang;
}

static uint32_t HandAlias(PieceKind kind) {
    if (IsLeftHandItem(kind)) {
        return kLeftFist;
    }
    return kind == PieceKind::FairyOcarina || kind == PieceKind::Ocarina ? kRightHand : kRightFist;
}

static Mtx PositionMatrix(float roll, float pitch, float heading, float x, float y, float z) {
    constexpr float kDegrees = 3.14159265f / 180.f;
    float sr = std::sin(roll * kDegrees);
    float cr = std::cos(roll * kDegrees);
    float sp = std::sin(pitch * kDegrees);
    float cp = std::cos(pitch * kDegrees);
    float sh = std::sin(heading * kDegrees);
    float ch = std::cos(heading * kDegrees);
    Mtx matrix{};
    matrix.mf[0][0] = cp * ch;
    matrix.mf[0][1] = cp * sh;
    matrix.mf[0][2] = -sp;
    matrix.mf[1][0] = sr * sp * ch - cr * sh;
    matrix.mf[1][1] = sr * sp * sh + cr * ch;
    matrix.mf[1][2] = sr * cp;
    matrix.mf[2][0] = cr * sp * ch + sr * sh;
    matrix.mf[2][1] = cr * sp * sh - sr * ch;
    matrix.mf[2][2] = cr * cp;
    matrix.mf[3][0] = x;
    matrix.mf[3][1] = y;
    matrix.mf[3][2] = z;
    matrix.mf[3][3] = 1.f;
    return matrix;
}

static const Mtx* DefaultSwordBack(bool child) {
    static const Mtx adult = PositionMatrix(0, 0, 0, -715, -310, 78);
    static const Mtx young = PositionMatrix(0, 0, 0, -440, -211, 0);
    return child ? &young : &adult;
}

static const Mtx* DefaultShieldBack(bool child) {
    static const Mtx adult = PositionMatrix(0, 0, 180, 935, 94, 29);
    static const Mtx young = PositionMatrix(0, 0, 180, 545, 0, 80);
    return child ? &young : &adult;
}

struct Part {
    Gfx* list;
    const Mtx* matrix = nullptr;
};

// Built lists are cached for the whole session: queued frames keep pointing at them.
class Composer {
  public:
    Gfx* Compose(const std::vector<Part>& parts) {
        std::vector<uintptr_t> key;
        for (const Part& part : parts) {
            key.push_back(reinterpret_cast<uintptr_t>(part.list));
            key.push_back(reinterpret_cast<uintptr_t>(part.matrix));
        }
        auto found = lists.find(key);
        if (found != lists.end()) {
            return found->second.data();
        }
        std::vector<Gfx> commands;
        for (const Part& part : parts) {
            if (part.list == nullptr) {
                continue;
            }
            if (part.matrix != nullptr) {
                commands.push_back(Command(0xDA380000, reinterpret_cast<uintptr_t>(part.matrix)));
            }
            commands.push_back(Command(0xDE000000, reinterpret_cast<uintptr_t>(part.list)));
            if (part.matrix != nullptr) {
                commands.push_back(Command(0xD8380002, 0x40));
            }
        }
        commands.push_back(Command(0xDF000000, 0));
        return lists.emplace(std::move(key), std::move(commands)).first->second.data();
    }

  private:
    std::map<std::vector<uintptr_t>, std::vector<Gfx>> lists;

    static Gfx Command(uintptr_t w0, uintptr_t w1) {
        Gfx command{};
        command.words.w0 = w0;
        command.words.w1 = w1;
        return command;
    }
};
} // namespace pak
