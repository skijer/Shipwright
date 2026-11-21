#pragma once

#include <cstdint>
#include <optional>
#include <string_view>
#include <unordered_map>

#include "soh/Enhancements/randomizer/randomizerEnumStrings.h"

template <typename T> std::optional<T> RandoEnumFromName(std::string_view name) {
    static const std::unordered_map<std::string_view, T> byName = [] {
        std::unordered_map<std::string_view, T> names;
        for (int32_t value = 0;; value++) {
            std::string_view text = EnumToString(static_cast<T>(value));
            if (text.empty()) {
                return names;
            }
            names.emplace(text, static_cast<T>(value));
        }
    }();
    auto found = byName.find(name);
    if (found == byName.end()) {
        return std::nullopt;
    }
    return found->second;
}
