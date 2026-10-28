#pragma once

#include <cstddef>
#include <memory>
#include <optional>
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

// The store catalog is signed with the mod-signing key, release manifests with the release key.
enum class SignedDocumentKind {
    Catalog,
    Release,
};

ModProvenance ModTrust_Verify(Ship::Archive& archive);
std::optional<std::string> ModTrust_OpenSignedDocument(const std::string& document, SignedDocumentKind kind);
std::string ModTrust_HashBytes(const void* data, size_t size);
std::vector<std::string> ModTrust_ListUnusualImports(const std::vector<char>& binary);
std::string ModTrust_Describe(const ModProvenance& provenance);
