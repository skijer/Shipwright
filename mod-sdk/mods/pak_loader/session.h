#pragma once
#include "library.h"

namespace pak {
struct ChoiceList {
    std::vector<std::string> keys;
    std::vector<std::string> labels;
    std::vector<const char*> values;

    explicit ChoiceList(const char* defaultLabel) {
        Add("", defaultLabel);
    }

    void Add(const std::string& key, const std::string& label) {
        keys.push_back(key);
        labels.push_back(label);
    }

    // Call once all entries are added: the pointers must not move afterwards.
    void Finish() {
        values.clear();
        for (const auto& label : labels) {
            values.push_back(label.c_str());
        }
    }

    int32_t IndexOf(const std::string& key) const {
        auto found = std::find(keys.begin(), keys.end(), key);
        return found == keys.end() ? 0 : int32_t(found - keys.begin());
    }

    void Sync(const char* indexCVar, const char* keyCVar) const {
        CVarSetInteger(indexCVar, IndexOf(CVarGetString(keyCVar, "")));
    }

    std::string Apply(const char* indexCVar, const char* keyCVar) const {
        int32_t index = CVarGetInteger(indexCVar, 0);
        if (index < 0 || size_t(index) >= keys.size()) {
            index = 0;
        }
        CVarSetString(keyCVar, keys[index].c_str());
        CVarSave();
        return keys[index];
    }
};

struct EquipSlot {
    PieceKind kind;
    int variant;
    const char* label;
    std::string keyCVar;
    std::string indexCVar;
    ChoiceList choices{ "From the Link model" };
    Piece* selected = nullptr;
};

static void BuildEquipSlot(EquipSlot& slot, const Library& library) {
    std::string key = SlotKey(slot.kind, slot.variant);
    slot.keyCVar = "gMods.PakLoader.Equip." + key;
    slot.indexCVar = "gMods.PakLoader.EquipMenu." + key;
    for (const auto& piece : library.pieces) {
        if (piece->kind == slot.kind) {
            slot.choices.Add(piece->key, piece->label);
        }
    }
    slot.choices.Finish();
    slot.choices.Sync(slot.indexCVar.c_str(), slot.keyCVar.c_str());
    slot.selected = library.FindPiece(CVarGetString(slot.keyCVar.c_str(), ""));
}

static Piece* FindEquipped(const std::vector<EquipSlot>& slots, const HeldItem& item) {
    for (const auto& slot : slots) {
        if (slot.kind == item.kind && slot.variant == item.variant) {
            return slot.selected;
        }
    }
    return nullptr;
}

static bool HasEquipped(const std::vector<EquipSlot>& slots) {
    return std::any_of(slots.begin(), slots.end(), [](const EquipSlot& slot) { return slot.selected != nullptr; });
}

static std::string ResourcePath(const std::string& original) {
    static constexpr char kPrefix[] = "__OTR__";
    return original.rfind(kPrefix, 0) == 0 ? original.substr(sizeof(kPrefix) - 1) : original;
}

static std::string Directory(const std::string& path) {
    size_t slash = path.rfind('/');
    return slash == std::string::npos ? "" : path.substr(0, slash + 1);
}

static std::string Leaf(const std::string& path) {
    size_t slash = path.rfind('/');
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

static uint32_t NextRoll() {
    static uint32_t state = 0x9E3779B9u;
    state = state * 1664525u + 1013904223u;
    return state >> 8;
}
} // namespace pak
