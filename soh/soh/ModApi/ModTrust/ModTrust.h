#pragma once

#include <memory>
#include <string>
#include <vector>

namespace Ship {
class Archive;
}

enum class ModTrustState {
    Verified,
    Unsigned,
    Invalid,
};

struct ModProvenance {
    ModTrustState state = ModTrustState::Unsigned;
    std::string repository;
    std::string commit;
    std::string author;
    std::string workflowRun;
    std::string contentHash;
    std::string problem;
};

ModProvenance ModTrust_Verify(Ship::Archive& archive);
std::vector<std::string> ModTrust_ListUnusualImports(const std::vector<char>& binary);
std::string ModTrust_Describe(const ModProvenance& provenance);
