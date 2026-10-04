#pragma once

#include <filesystem>
#include <string>

namespace PakSelection {

// Relative to mods/, not just the basename: subfolders may contain namesakes,
// and relocating the whole game directory must not change a selection.
inline std::string RelativePath(const std::filesystem::path& file, const std::filesystem::path& root) {
    std::error_code ec;
    auto absoluteFile = std::filesystem::absolute(file, ec);
    if (ec) {
        return file.lexically_normal().generic_string();
    }
    auto absoluteRoot = std::filesystem::absolute(root, ec);
    if (!ec) {
        auto relative = absoluteFile.lexically_normal().lexically_relative(absoluteRoot.lexically_normal());
        if (!relative.empty()) {
            return relative.generic_string();
        }
    }
    return absoluteFile.lexically_normal().generic_string();
}

// A missing Path CVar is a legacy config. Once migrated, the path is authoritative:
// an absent/incompatible file means Default, never whichever model inherited its index.
template <typename PathForIndex, typename IsCompatible>
int ResolveIndex(int legacyIndex, const char* savedPath, int count, PathForIndex pathForIndex,
                 IsCompatible isCompatible) {
    if (savedPath != nullptr) {
        if (savedPath[0] != '\0') {
            for (int i = 0; i < count; ++i) {
                if (isCompatible(i) && pathForIndex(i) == savedPath) {
                    return i;
                }
            }
        }
        return -1;
    }
    return legacyIndex >= 0 && legacyIndex < count && isCompatible(legacyIndex) ? legacyIndex : -1;
}

} // namespace PakSelection
