#include "ModTrust.h"
#include "RevokedMods.h"
#include "TrustedModKey.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <optional>
#include <set>
#include <string_view>

#include <nlohmann/json.hpp>

#include <ship/resource/File.h>
#include <ship/resource/archive/Archive.h>

#include "monocypher/monocypher.h"
#include "monocypher/monocypher-ed25519.h"

namespace {

using Json = nlohmann::json;

constexpr const char* SignatureFileName = "unbound-signature.json";
constexpr const char* SignatureDomain = "unbound-mod-signature-v1";
constexpr size_t ContentHashSize = 64;
constexpr size_t SignatureSize = 64;
constexpr size_t MaxImportedLibraries = 256;
constexpr size_t MaxImportsPerLibrary = 4096;

// Same lists as mod-sdk/tools/check_mod_binary.py, which enforces them before signing.
constexpr std::string_view RuntimeLibraryPrefixes[] = { "vcruntime140", "msvcp140", "ucrtbase", "api-ms-win-crt-" };
constexpr std::string_view StartupKernelImports[] = {
    "DisableThreadLibraryCalls",   "GetCurrentProcess",   "GetCurrentProcessId",      "GetCurrentThreadId",
    "GetSystemTimeAsFileTime",     "InitializeSListHead", "IsDebuggerPresent",        "IsProcessorFeaturePresent",
    "QueryPerformanceCounter",     "RtlCaptureContext",   "RtlLookupFunctionEntry",   "RtlVirtualUnwind",
    "SetUnhandledExceptionFilter", "TerminateProcess",    "UnhandledExceptionFilter",
};
constexpr std::string_view DangerousRuntimeImports[] = {
    "system",   "_wsystem",  "_popen",  "_wpopen",   "fopen",    "fopen_s",      "_wfopen",      "_wfopen_s",
    "_fsopen",  "_wfsopen",  "freopen", "freopen_s", "_open",    "_wopen",       "_sopen",       "_sopen_s",
    "_wsopen",  "_wsopen_s", "_creat",  "remove",    "_wremove", "rename",       "_wrename",     "_unlink",
    "_wunlink", "_rmdir",    "_wrmdir", "_mkdir",    "_wmkdir",  "_Thrd_create", "_beginthread", "_beginthreadex",
};
constexpr std::string_view HostTrustInternals[] = { "ModPermissions_", "ModLoader_",
                                                    "ModTrust_",       "UserFiles_ApplyPending",
                                                    "crypto_",         "kTrustedModSigningKey",
                                                    "kRevokedMod" };
constexpr std::string_view DangerousRuntimeImportPrefixes[] = { "_execl", "_execv",  "_wexecl",   "_wexecv",
                                                                "_spawn", "_wspawn", "__std_fs_", "?_Fiopen" };

struct PeSection {
    uint32_t virtualAddress;
    uint32_t virtualSize;
    uint32_t rawOffset;
    uint32_t rawSize;
};

struct PeImport {
    std::string library;
    std::string function;
};

std::string ToHex(const uint8_t* bytes, size_t size) {
    constexpr char digits[] = "0123456789abcdef";
    std::string text;
    text.reserve(size * 2);
    for (size_t i = 0; i < size; ++i) {
        text.push_back(digits[bytes[i] >> 4]);
        text.push_back(digits[bytes[i] & 0xF]);
    }
    return text;
}

std::optional<std::vector<uint8_t>> FromHex(const std::string& text, size_t size) {
    if (text.size() != size * 2) {
        return std::nullopt;
    }
    std::vector<uint8_t> bytes(size);
    for (size_t i = 0; i < size; ++i) {
        const std::string pair = text.substr(i * 2, 2);
        if (!std::isxdigit(static_cast<unsigned char>(pair[0])) ||
            !std::isxdigit(static_cast<unsigned char>(pair[1]))) {
            return std::nullopt;
        }
        bytes[i] = static_cast<uint8_t>(std::stoul(pair, nullptr, 16));
    }
    return bytes;
}

void HashLittleEndian(crypto_blake2b_ctx& context, uint64_t value, size_t byteCount) {
    uint8_t encoded[8];
    for (size_t i = 0; i < byteCount; ++i) {
        encoded[i] = static_cast<uint8_t>(value >> (8 * i));
    }
    crypto_blake2b_update(&context, encoded, byteCount);
}

// Each file is hashed as u32 path length, path, u64 data length, data, in byte order of the paths.
std::optional<std::string> ComputeContentHash(Ship::Archive& archive) {
    std::vector<std::string> paths;
    for (const auto& [hash, path] : *archive.ListFiles()) {
        if (path != SignatureFileName) {
            paths.push_back(path);
        }
    }
    std::sort(paths.begin(), paths.end());

    crypto_blake2b_ctx context;
    crypto_blake2b_init(&context, ContentHashSize);
    for (const std::string& path : paths) {
        auto file = archive.LoadFile(path);
        if (file == nullptr || !file->IsLoaded || file->Buffer == nullptr) {
            return std::nullopt;
        }
        HashLittleEndian(context, path.size(), 4);
        crypto_blake2b_update(&context, reinterpret_cast<const uint8_t*>(path.data()), path.size());
        HashLittleEndian(context, file->Buffer->size(), 8);
        crypto_blake2b_update(&context, reinterpret_cast<const uint8_t*>(file->Buffer->data()), file->Buffer->size());
    }

    uint8_t digest[ContentHashSize];
    crypto_blake2b_final(&context, digest);
    return ToHex(digest, ContentHashSize);
}

std::string ReadStatementField(const Json& statement, const char* key) {
    const auto field = statement.find(key);
    if (field == statement.end() || !field->is_string()) {
        return {};
    }
    return field->get<std::string>();
}

bool IsStatementField(const std::string& value) {
    return !value.empty() && value.find('\n') == std::string::npos;
}

bool HasTrustedKey() {
    return std::any_of(std::begin(kTrustedModSigningKey), std::end(kTrustedModSigningKey),
                       [](uint8_t byte) { return byte != 0; });
}

std::string BuildSignedMessage(const ModProvenance& provenance) {
    return std::string(SignatureDomain) + "\n" + provenance.repository + "\n" + provenance.commit + "\n" +
           provenance.author + "\n" + provenance.workflowRun + "\n" + provenance.contentHash;
}

ModProvenance Reject(ModProvenance provenance, const char* problem) {
    provenance.state = ModTrustState::Invalid;
    provenance.problem = problem;
    return provenance;
}

template <typename T> std::optional<T> ReadAt(const std::vector<char>& image, size_t offset) {
    if (offset > image.size() || image.size() - offset < sizeof(T)) {
        return std::nullopt;
    }
    T value;
    std::memcpy(&value, image.data() + offset, sizeof(T));
    return value;
}

std::optional<size_t> RvaToOffset(const std::vector<PeSection>& sections, uint32_t rva) {
    for (const PeSection& section : sections) {
        if (rva < section.virtualAddress) {
            continue;
        }
        const uint32_t delta = rva - section.virtualAddress;
        if (delta < std::max(section.virtualSize, section.rawSize)) {
            return delta < section.rawSize ? std::optional<size_t>(size_t(section.rawOffset) + delta) : std::nullopt;
        }
    }
    return std::nullopt;
}

std::string ReadName(const std::vector<char>& image, size_t offset) {
    std::string name;
    while (offset < image.size() && image[offset] != '\0' && name.size() < 512) {
        name.push_back(image[offset]);
        ++offset;
    }
    return name;
}

std::optional<std::vector<PeSection>> ReadPeSections(const std::vector<char>& image, size_t sectionTable,
                                                     uint16_t sectionCount) {
    std::vector<PeSection> sections;
    for (uint16_t i = 0; i < sectionCount; ++i) {
        const size_t entry = sectionTable + size_t(i) * 40;
        auto virtualSize = ReadAt<uint32_t>(image, entry + 8);
        auto virtualAddress = ReadAt<uint32_t>(image, entry + 12);
        auto rawSize = ReadAt<uint32_t>(image, entry + 16);
        auto rawOffset = ReadAt<uint32_t>(image, entry + 20);
        if (!virtualSize || !virtualAddress || !rawSize || !rawOffset) {
            return std::nullopt;
        }
        sections.push_back({ *virtualAddress, *virtualSize, *rawOffset, *rawSize });
    }
    return sections;
}

void ReadLibraryImports(const std::vector<char>& image, const std::vector<PeSection>& sections, bool is64,
                        const std::string& library, size_t thunkTable, std::vector<PeImport>& imports) {
    const size_t thunkSize = is64 ? 8 : 4;
    const uint64_t ordinalFlag = is64 ? (1ULL << 63) : (1ULL << 31);
    for (size_t i = 0; i < MaxImportsPerLibrary; ++i) {
        const size_t offset = thunkTable + i * thunkSize;
        const uint64_t thunk =
            is64 ? ReadAt<uint64_t>(image, offset).value_or(0) : ReadAt<uint32_t>(image, offset).value_or(0);
        if (thunk == 0) {
            return;
        }
        if (thunk & ordinalFlag) {
            imports.push_back({ library, "#" + std::to_string(thunk & 0xFFFF) });
            continue;
        }
        auto hintName = RvaToOffset(sections, static_cast<uint32_t>(thunk));
        imports.push_back({ library, hintName ? ReadName(image, *hintName + 2) : "?" });
    }
}

std::vector<PeImport> ReadPeImports(const std::vector<char>& image) {
    std::vector<PeImport> imports;
    auto dosMagic = ReadAt<uint16_t>(image, 0);
    auto peOffset = ReadAt<uint32_t>(image, 0x3C);
    if (!dosMagic || *dosMagic != 0x5A4D || !peOffset || ReadAt<uint32_t>(image, *peOffset) != 0x00004550u) {
        return imports;
    }

    const size_t fileHeader = size_t(*peOffset) + 4;
    const size_t optionalHeader = fileHeader + 20;
    auto sectionCount = ReadAt<uint16_t>(image, fileHeader + 2);
    auto optionalHeaderSize = ReadAt<uint16_t>(image, fileHeader + 16);
    auto optionalMagic = ReadAt<uint16_t>(image, optionalHeader);
    if (!sectionCount || !optionalHeaderSize || !optionalMagic) {
        return imports;
    }
    const bool is64 = *optionalMagic == 0x20B;
    auto importTableRva = ReadAt<uint32_t>(image, optionalHeader + (is64 ? 120 : 104));
    auto sections = ReadPeSections(image, optionalHeader + *optionalHeaderSize, *sectionCount);
    if (!importTableRva || !sections) {
        return imports;
    }

    auto importTable = RvaToOffset(*sections, *importTableRva);
    for (size_t i = 0; importTable && i < MaxImportedLibraries; ++i) {
        const size_t descriptor = *importTable + i * 20;
        auto lookupRva = ReadAt<uint32_t>(image, descriptor);
        auto nameRva = ReadAt<uint32_t>(image, descriptor + 12);
        auto addressRva = ReadAt<uint32_t>(image, descriptor + 16);
        if (!lookupRva || !nameRva || !addressRva || *nameRva == 0) {
            break;
        }
        auto name = RvaToOffset(*sections, *nameRva);
        auto thunkTable = RvaToOffset(*sections, *lookupRva != 0 ? *lookupRva : *addressRva);
        if (!name || !thunkTable) {
            break;
        }
        ReadLibraryImports(image, *sections, is64, ReadName(image, *name), *thunkTable, imports);
    }
    return imports;
}

std::string Lowercase(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return text;
}

template <typename List> bool Contains(const List& list, std::string_view value) {
    return std::find(std::begin(list), std::end(list), value) != std::end(list);
}

bool IsRevoked(const ModProvenance& provenance) {
    return Contains(kRevokedModAuthors, Lowercase(provenance.author)) ||
           Contains(kRevokedModPackages, provenance.contentHash);
}

template <size_t N> bool StartsWithAny(const std::string_view (&prefixes)[N], std::string_view value) {
    return std::any_of(std::begin(prefixes), std::end(prefixes),
                       [&](std::string_view prefix) { return value.substr(0, prefix.size()) == prefix; });
}

template <size_t N> bool ContainsAny(const std::string_view (&parts)[N], std::string_view value) {
    return std::any_of(std::begin(parts), std::end(parts),
                       [&](std::string_view part) { return value.find(part) != std::string_view::npos; });
}

} // namespace

ModProvenance ModTrust_Verify(Ship::Archive& archive) {
    ModProvenance provenance;
    auto contentHash = ComputeContentHash(archive);
    if (!contentHash) {
        return Reject(provenance, "a file in the archive could not be read");
    }
    provenance.contentHash = *contentHash;

    auto statementFile = archive.LoadFile(SignatureFileName);
    if (statementFile == nullptr || !statementFile->IsLoaded || statementFile->Buffer == nullptr) {
        provenance.problem = "not signed";
        return provenance;
    }

    const Json statement = Json::parse(statementFile->Buffer->begin(), statementFile->Buffer->end(), nullptr, false);
    if (!statement.is_object()) {
        return Reject(provenance, "the signature file is not valid JSON");
    }
    provenance.repository = ReadStatementField(statement, "repository");
    provenance.commit = ReadStatementField(statement, "commit");
    provenance.author = ReadStatementField(statement, "author");
    provenance.workflowRun = ReadStatementField(statement, "workflow_run");
    const std::string signedContentHash = ReadStatementField(statement, "content_hash");
    auto signature = FromHex(ReadStatementField(statement, "signature"), SignatureSize);

    const bool complete = IsStatementField(provenance.repository) && IsStatementField(provenance.commit) &&
                          IsStatementField(provenance.author) && IsStatementField(provenance.workflowRun);
    if (!complete || !signature) {
        return Reject(provenance, "the signature file is incomplete");
    }
    if (signedContentHash != provenance.contentHash) {
        return Reject(provenance, "files changed after signing");
    }
    if (!HasTrustedKey()) {
        return Reject(provenance, "this build carries no mod-signing key");
    }

    const std::string message = BuildSignedMessage(provenance);
    if (crypto_ed25519_check(signature->data(), kTrustedModSigningKey, reinterpret_cast<const uint8_t*>(message.data()),
                             message.size()) != 0) {
        return Reject(provenance, "the signature is not from Unbound's sign-mod workflow");
    }
    if (IsRevoked(provenance)) {
        return Reject(provenance, "revoked by Unbound");
    }
    provenance.state = ModTrustState::Verified;
    return provenance;
}

std::vector<std::string> ModTrust_ListUnusualImports(const std::vector<char>& binary) {
    std::set<std::string> unusual;
    for (const PeImport& import : ReadPeImports(binary)) {
        const std::string library = Lowercase(import.library);
        if (library == "soh.exe") {
            if (ContainsAny(HostTrustInternals, import.function)) {
                unusual.insert(import.library + "!" + import.function);
            }
            continue;
        }
        if (library == "kernel32.dll") {
            if (!Contains(StartupKernelImports, import.function)) {
                unusual.insert(import.library + "!" + import.function);
            }
            continue;
        }
        if (!StartsWithAny(RuntimeLibraryPrefixes, library)) {
            unusual.insert(import.library);
            continue;
        }
        if (Contains(DangerousRuntimeImports, import.function) ||
            StartsWithAny(DangerousRuntimeImportPrefixes, import.function)) {
            unusual.insert(import.library + "!" + import.function);
        }
    }
    return { unusual.begin(), unusual.end() };
}

std::string ModTrust_Describe(const ModProvenance& provenance) {
    switch (provenance.state) {
        case ModTrustState::Verified:
            return "signed: " + provenance.repository + "@" + provenance.commit.substr(0, 7) + " by " +
                   provenance.author;
        case ModTrustState::Unsigned:
            return "unsigned";
        case ModTrustState::Invalid:
            return "invalid signature: " + provenance.problem;
        default:
            return "unknown trust state";
    }
}
