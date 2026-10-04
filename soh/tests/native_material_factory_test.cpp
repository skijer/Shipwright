// Exercise the production decorator and libultraship's binary DL parser. Only
// archive I/O/lookup is replaced with in-memory metadata and physical path owners.
#include "../soh/Enhancements/Graphics/PreludeNativeMaterialScroll.cpp"
#include "test_require.h"
#include <fast/lus_gbi.h>
#include <atomic>
#include <fstream>
#include <iostream>
#include <latch>
#include <thread>

#include "native_binary_display_list_factory.inc"

static std::map<std::string, std::shared_ptr<Ship::Archive>> sOwners;
namespace Ship {
Archive::Archive(const std::string& path) : mPath(path) {
}
Archive::~Archive() = default;
const std::string& Archive::GetPath() {
    return mPath;
}
bool Archive::HasFile(const std::string& path) {
    return path == "prelude/project/edits.json";
}
ArchiveManager::ArchiveManager() = default;
ArchiveManager::~ArchiveManager() = default;
std::shared_ptr<Archive> ArchiveManager::GetArchiveFromFile(const std::string& path) {
    const auto owner = sOwners.find(path);
    return owner == sOwners.end() ? nullptr : owner->second;
}
} // namespace Ship

class MetadataArchive : public Ship::Archive {
  public:
    MetadataArchive() : Archive("fixture") {
    }
    nlohmann::json project;
    std::atomic<size_t> metadataReads = 0;
    bool failNextRead = false;
    bool truncateNextRead = false;
    std::shared_ptr<Ship::File> LoadFile(const std::string& path) override {
        REQUIRE(path == "prelude/project/edits.json");
        ++metadataReads;
        if (failNextRead) {
            failNextRead = false;
            return nullptr;
        }
        auto file = std::make_shared<Ship::File>();
        auto text = project.dump();
        if (truncateNextRead) {
            truncateNextRead = false;
            text.resize(text.size() / 2);
        }
        file->Buffer = std::make_shared<std::vector<char>>(text.begin(), text.end());
        return file;
    }
    std::shared_ptr<Ship::File> LoadFile(uint64_t) override {
        return nullptr;
    }
    bool Open() override {
        return true;
    }
    bool Close() override {
        return true;
    }
    bool WriteFile(const std::string&, const std::vector<uint8_t>&) override {
        return false;
    }
};

static std::shared_ptr<Fast::DisplayList> ReadDisplayList(Prelude::NativeMaterialDisplayListFactory& factory,
                                                          const std::string& path,
                                                          const std::vector<Prelude::NativeMaterialCommand>& commands,
                                                          const std::shared_ptr<Ship::Archive>& parent = nullptr,
                                                          UcodeHandlers ucode = ucode_f3dex2) {
    auto init = std::make_shared<Ship::ResourceInitData>();
    init->Path = path;
    init->Parent = parent;
    init->ByteOrder = Ship::Endianness::Little;
    init->Type = 0x4f444c54; // Binary display list, "TLDO" on disk.
    init->ResourceVersion = 0;
    init->Id = 0;
    init->IsCustom = path.starts_with("alt/");
    init->Format = RESOURCE_FORMAT_BINARY;
    auto file = std::make_shared<Ship::File>();
    file->Buffer = std::make_shared<std::vector<char>>(8, char(0xff));
    (*file->Buffer)[0] = static_cast<char>(ucode);
    for (const auto& command : commands) {
        for (uintptr_t word : { command.w0, command.w1 }) {
            for (unsigned shift : { 0u, 8u, 16u, 24u }) {
                file->Buffer->push_back(static_cast<char>(word >> shift));
            }
        }
    }
    auto reader = std::make_shared<Ship::BinaryReader>(file->Buffer->data(), file->Buffer->size());
    reader->SetEndianness(init->ByteOrder);
    file->Reader = reader;
    file->IsLoaded = true;
    auto result = std::dynamic_pointer_cast<Fast::DisplayList>(factory.ReadResource(file, init));
    REQUIRE(result);
    REQUIRE(result->GetInitData()->Path == path); // Physical resource identity stays intact.
    return result;
}

static void CheckBinding(const std::shared_ptr<Fast::DisplayList>& result,
                         const std::vector<Prelude::NativeMaterialCommand>& commands,
                         Prelude::NativeMaterialProfile profile, std::optional<size_t> insertion) {
    const size_t expectedSize = commands.size() + (insertion ? 1 : 0);
    if (result->Instructions.size() != expectedSize) {
        std::cerr << result->GetInitData()->Path << ": expected " << expectedSize << " instructions, got "
                  << result->Instructions.size() << '\n';
    }
    REQUIRE(result->Instructions.size() == expectedSize);
    if (insertion) {
        REQUIRE(*insertion < commands.size());
        const Gfx call = gsSPDisplayList(Prelude::Lists().lists[static_cast<size_t>(profile)].data());
        REQUIRE(result->Instructions[*insertion].words.w0 == call.words.w0);
        REQUIRE(result->Instructions[*insertion].words.w1 == call.words.w1);
    }
    for (size_t i = 0; i < commands.size(); ++i) {
        const auto& actual = result->Instructions[i + (insertion && i >= *insertion ? 1 : 0)];
        REQUIRE(actual.words.w0 == commands[i].w0);
        REQUIRE(actual.words.w1 == commands[i].w1);
    }
}

static void CheckAlternateOwnership() {
    const std::string path = "custom/prelude/test/sage_platform";
    const std::string altPath = "alt/" + path;
    const std::vector<Prelude::NativeMaterialCommand> material = {
        { 0xf5101000, 0x00017c5e }, { 0xf2000000, 0x0007c07c }, { 0xf5101000, 0x0101785f },
        { 0xf2000000, 0x0107c07c }, { 0xdf000000, 0 },
    };
    const auto profile = Prelude::NativeMaterialProfile::ChamberOfSagesPlatform;
    auto bound = std::make_shared<MetadataArchive>();
    bound->project = nlohmann::json::parse(
        R"({"edits":{"test":[{"data":{"materials":[{"newDlPath":"custom/prelude/test/sage_platform","nativeAnimation":{"version":1,"binding":"material-motion","source":"oot.chamber_of_sages.platform","logicalWidth":32,"logicalHeight":32}}]}}]}})");
    auto unbound = std::make_shared<MetadataArchive>();
    unbound->project = bound->project;
    unbound->project["edits"]["test"][0]["data"]["materials"][0].erase("nativeAnimation");
    auto archives = std::make_shared<Ship::ArchiveManager>();
    Prelude::NativeMaterialDisplayListFactory factory(archives);

    sOwners = { { path, bound }, { altPath, bound } };
    CheckBinding(ReadDisplayList(factory, path, material), material, profile, 4);
    CheckBinding(ReadDisplayList(factory, altPath, material), material, profile, 4);

    // The canonical path is owned by a different archive. The alternate list
    // may borrow the canonical metadata key, but never that archive's metadata.
    sOwners = { { path, bound }, { altPath, unbound } };
    CheckBinding(ReadDisplayList(factory, altPath, material), material, profile, std::nullopt);
    sOwners = { { path, unbound }, { altPath, bound } };
    CheckBinding(ReadDisplayList(factory, altPath, material), material, profile, 4);

    // An explicitly supplied parent remains authoritative over path lookup.
    CheckBinding(ReadDisplayList(factory, altPath, material, unbound), material, profile, std::nullopt);
    sOwners[altPath] = unbound;
    CheckBinding(ReadDisplayList(factory, altPath, material, bound), material, profile, 4);
    CheckBinding(ReadDisplayList(factory, "alt/" + altPath, material, bound), material, profile, std::nullopt);
    sOwners.clear();
}

static void ProbeArchive(const char* fixturePath) {
    std::ifstream input(fixturePath);
    const auto fixture = nlohmann::json::parse(input);
    auto owner = std::make_shared<MetadataArchive>();
    owner->project = fixture["project"];
    Prelude::NativeMaterialDisplayListFactory factory(std::make_shared<Ship::ArchiveManager>());
    nlohmann::json output = nlohmann::json::array();
    for (const auto& item : fixture["resources"]) {
        const std::string path = item["path"];
        sOwners[path] = owner;
        std::vector<Prelude::NativeMaterialCommand> commands;
        for (const auto& pair : item["commands"]) {
            commands.push_back({ pair[0].get<uintptr_t>(), pair[1].get<uintptr_t>() });
        }
        const auto profile = static_cast<Prelude::NativeMaterialProfile>(item["profile"].get<int>());
        const auto insertion =
            item["insertion"].is_null() ? std::nullopt : std::optional<size_t>(item["insertion"].get<size_t>());
        auto result =
            ReadDisplayList(factory, path, commands, nullptr, static_cast<UcodeHandlers>(item["ucode"].get<int>()));
        CheckBinding(result, commands, profile, insertion);
        output.push_back({ { "path", path },
                           { "profile", static_cast<int>(profile) },
                           { "insertion", item["insertion"] },
                           { "instruction_count", result->Instructions.size() },
                           { "adapter_verified", true } });
    }
    REQUIRE(owner->metadataReads <= 1);
    sOwners.clear();
    std::cerr << "PASS native archive factory: " << output.size() << " lists, " << owner->metadataReads.load()
              << " metadata read(s)\n";
    std::cout << output.dump(2) << '\n';
}

static void CheckMetadataReadReuse() {
    const std::string path = "custom/prelude/test/sage_platform";
    const std::vector<Prelude::NativeMaterialCommand> material = {
        { 0xf5101000, 0x00017c5e }, { 0xf2000000, 0x0007c07c }, { 0xf5101000, 0x0101785f },
        { 0xf2000000, 0x0107c07c }, { 0xdf000000, 0 },
    };
    auto owner = std::make_shared<MetadataArchive>();
    owner->project = nlohmann::json::parse(
        R"({"edits":{"test":[{"data":{"materials":[{"newDlPath":"custom/prelude/test/sage_platform","nativeAnimation":{"version":1,"binding":"material-motion","source":"oot.chamber_of_sages.platform","logicalWidth":32,"logicalHeight":32}}]}}]}})");
    Prelude::NativeMaterialDisplayListFactory factory(std::make_shared<Ship::ArchiveManager>());
    const auto profile = Prelude::NativeMaterialProfile::ChamberOfSagesPlatform;
    CheckBinding(ReadDisplayList(factory, path, material, owner), material, profile, 4);
    CheckBinding(ReadDisplayList(factory, "alt/" + path, material, owner), material, profile, 4);
    // A room's unbound geometry also passes through the decorator. Importing
    // more lists from this archive must not reread its entire project JSON.
    for (size_t i = 0; i < 32; ++i) {
        CheckBinding(ReadDisplayList(factory, "custom/prelude/test/geometry" + std::to_string(i), material, owner),
                     material, profile, std::nullopt);
    }
    REQUIRE(owner->metadataReads == 1);

    // Reopening the same archive object must refresh removed bindings. It must
    // not depend on a different owner pointer or a filesystem timestamp change.
    const auto originalProject = owner->project;
    owner->project["edits"]["test"][0]["data"]["materials"][0].erase("nativeAnimation");
    PreludeNativeMaterialScroll_InvalidateMetadata("test refresh");
    CheckBinding(ReadDisplayList(factory, path, material, owner), material, profile, std::nullopt);
    CheckBinding(ReadDisplayList(factory, "alt/" + path, material, owner), material, profile, std::nullopt);
    REQUIRE(owner->metadataReads == 2);

    owner->project = originalProject;
    PreludeNativeMaterialScroll_InvalidateMetadata("test refresh");
    CheckBinding(ReadDisplayList(factory, path, material, owner), material, profile, 4);
    REQUIRE(owner->metadataReads == 3);

    auto retry = std::make_shared<MetadataArchive>();
    retry->project = originalProject;
    retry->failNextRead = true;
    CheckBinding(ReadDisplayList(factory, path, material, retry), material, profile, std::nullopt);
    CheckBinding(ReadDisplayList(factory, path, material, retry), material, profile, 4);
    CheckBinding(ReadDisplayList(factory, path, material, retry), material, profile, 4);
    REQUIRE(retry->metadataReads == 2);

    // O2rArchive can return a non-null buffer after a failed/short ZIP read.
    // Malformed JSON must not turn that transient failure into a cached miss.
    auto truncated = std::make_shared<MetadataArchive>();
    truncated->project = originalProject;
    truncated->truncateNextRead = true;
    CheckBinding(ReadDisplayList(factory, path, material, truncated), material, profile, std::nullopt);
    CheckBinding(ReadDisplayList(factory, path, material, truncated), material, profile, 4);
    CheckBinding(ReadDisplayList(factory, path, material, truncated), material, profile, 4);
    REQUIRE(truncated->metadataReads == 2);

    // Resource workers can reach the same archive simultaneously on first use.
    auto concurrent = std::make_shared<MetadataArchive>();
    concurrent->project = originalProject;
    std::latch start(1);
    std::vector<std::thread> readers;
    for (size_t i = 0; i < 8; ++i) {
        readers.emplace_back([&] {
            start.wait();
            REQUIRE(Prelude::ProfileFor(concurrent, path) == profile);
        });
    }
    start.count_down();
    for (auto& reader : readers) {
        reader.join();
    }
    REQUIRE(concurrent->metadataReads == 1);
}

static void CheckRepeatedMetadataReads() {
    auto owner = std::make_shared<MetadataArchive>();
    owner->project = { { "edits", nlohmann::json::object() } };
    Prelude::NativeMaterialDisplayListFactory factory(std::make_shared<Ship::ArchiveManager>());
    Prelude::LoadProbe::BeginFrame(0x5b, 9, 0, 1, true, true);
    for (size_t i = 0; i < 32; ++i) {
        REQUIRE(ReadDisplayList(factory, "alt/custom/prelude/test/unbound" + std::to_string(i), { { 0xdf000000, 0 } },
                                owner) != nullptr);
    }
    std::cerr << "Unbound imports: 32; metadata reads: " << owner->metadataReads << '\n';
    REQUIRE(owner->metadataReads == 1);
    auto report = Prelude::LoadProbe::EndFrame();
    REQUIRE(report.has_value());
    REQUIRE((*report)["material_lookups"] == 32);
    REQUIRE((*report)["metadata_reads"] == 1);
    REQUIRE((*report)["metadata_cache_misses"] == 1);
    REQUIRE((*report)["metadata_cache_hits"] == 31);

    // A refresh between frames is visible in the generation even though it
    // falls outside the measured frame. The next import must read again.
    const auto generation = (*report)["metadata_cache_generation"].get<uint64_t>();
    PreludeNativeMaterialScroll_InvalidateMetadata("test between frames");
    Prelude::LoadProbe::BeginFrame(0x5b, 9, 0, 2, true, true);
    REQUIRE(ReadDisplayList(factory, "alt/custom/prelude/test/after_reload", { { 0xdf000000, 0 } }, owner) != nullptr);
    report = Prelude::LoadProbe::EndFrame();
    REQUIRE(report.has_value());
    REQUIRE((*report)["metadata_cache_generation"] == generation + 1);
    REQUIRE((*report)["metadata_invalidations_during_frame"] == 0);
    REQUIRE((*report)["metadata_cache_misses"] == 1);
    REQUIRE(owner->metadataReads == 2);

    // ResourceManager normally executes the factory on its worker pool. Its
    // completed imports must be visible without pretending they ran on the
    // main thread or adding their elapsed work to frame_ms.
    auto workerOwner = std::make_shared<MetadataArchive>();
    workerOwner->project = { { "edits", nlohmann::json::object() } };
    Prelude::LoadProbe::BeginFrame(0x5b, 9, 0, 3, true, true);
    std::thread worker([&] {
        REQUIRE(ReadDisplayList(factory, "custom/prelude/test/worker0", { { 0xdf000000, 0 } }, workerOwner) != nullptr);
        REQUIRE(ReadDisplayList(factory, "custom/prelude/test/worker1", { { 0xdf000000, 0 } }, workerOwner) != nullptr);
    });
    worker.join();
    report = Prelude::LoadProbe::EndFrame();
    REQUIRE(report.has_value());
    REQUIRE((*report)["material_lookups"] == 0);
    REQUIRE((*report)["metadata_cache_hits"] == 0);
    REQUIRE((*report)["metadata_cache_misses"] == 0);
    const auto& completed = (*report)["all_threads_completed_work"];
    REQUIRE(completed["material_lookups"] == 2);
    REQUIRE(completed["metadata_cache_misses"] == 1);
    REQUIRE(completed["metadata_cache_hits"] == 1);
    REQUIRE(completed["metadata_reads"] == 1);
    REQUIRE(completed["binary_decodes"] == 2);
    REQUIRE(workerOwner->metadataReads == 1);
}

int main(int argc, char** argv) {
    spdlog::set_level(spdlog::level::off);
    if (argc == 2) {
        ProbeArchive(argv[1]);
        return 0;
    }
    REQUIRE(argc == 1);
    CheckAlternateOwnership();
    CheckMetadataReadReuse();
    CheckRepeatedMetadataReads();
    std::cout << "PASS native material factory ownership, metadata read reuse, reload, retry, and concurrent imports\n";
}
