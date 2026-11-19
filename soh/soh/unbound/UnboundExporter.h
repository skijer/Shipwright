#pragma once
#include <string>

namespace SOH::Unbound {

struct ExportReport {
    bool ok = false;
    size_t failures = 0;
    size_t scenes = 0;
    size_t rooms = 0;
    size_t copied = 0;
    size_t messages = 0;
    std::string error;
};

ExportReport ExportArchive(const std::string& outPath);

std::string ConverterName();

struct PortVersion {
    int major = -1;
    int minor = -1;
    int patch = -1;
    bool operator==(const PortVersion& other) const {
        return major == other.major && minor == other.minor && patch == other.patch;
    }
};

PortVersion MountedPortVersion();

inline constexpr const char* kBaseArchiveName = "oot-unbound.o2r";

enum class BaseArchiveState {
    None,
    Mounted,
    Converted,
};

// Call after the resource factories are registered and before mods are mounted.
BaseArchiveState EnsureBaseArchive(const std::string& gameArchiveDir);

// Call once it is mounted as the game archive and the resource factories are registered.
std::string CheckStandaloneBaseArchive();

} // namespace SOH::Unbound

extern "C" int Unbound_Export(const char* outPath);
