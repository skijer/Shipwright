#include "soh/Extractor/RomInfo.h"
#include "soh/Extractor/TorchExtract.h"

#include <emscripten.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/base_sink.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <vector>

namespace {

namespace fs = std::filesystem;

constexpr const char* kRecipeDir = "/work/assets";

// Covers the header CRC at 0x10 and the archive signatures.
constexpr size_t kHeaderBytes = 0x40;

enum class Status : int {
    Ok = 0,
    CannotRead = -1,
    BadSize = -2,
    Compressed = -3,
    UnknownVersion = -4,
    BadCrc = -5,
    ExtractFailed = -6,
    NoArchive = -7,
};

const char* Describe(Status status) {
    switch (status) {
        case Status::Ok:
            return "";
        case Status::CannotRead:
            return "The ROM file could not be read.";
        case Status::BadSize:
            return "The rom file was not a valid size. Expecting 32, 54, or 64MB.";
        case Status::Compressed:
            return "The selected file appears to be compressed. Please extract before using.";
        case Status::UnknownVersion:
            return "The rom's version is not one this build can extract. Please find another.";
        case Status::BadCrc:
            return "Rom CRC did not match the list of known compatible roms. Please find another.";
        case Status::ExtractFailed:
            return "Extraction failed.";
        case Status::NoArchive:
            return "Extraction finished but produced no archive.";
    }
    return "";
}

struct Result {
    Status status = Status::Ok;
    std::string detail;
    std::string version;
    std::string archive;
};

std::string gLastResultJson;

// The level has to be the sink's own under `quiet`, because Torch's Init resets every logger to debug.
class SplitConsoleSink final : public spdlog::sinks::base_sink<std::mutex> {
  protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        spdlog::memory_buf_t formatted;
        formatter_->format(msg, formatted);
        const bool problem = msg.level == spdlog::level::warn || msg.level == spdlog::level::err;
        FILE* out = problem ? stderr : stdout;
        fwrite(formatted.data(), 1, formatted.size(), out);
    }
    void flush_() override {
        fflush(stdout);
        fflush(stderr);
    }
};

void InstallLogger(bool quiet) {
    auto sink = std::make_shared<SplitConsoleSink>();
    if (quiet) {
        sink->set_level(spdlog::level::warn);
    }
    spdlog::set_default_logger(std::make_shared<spdlog::logger>("soh-extract", sink));
}

void ReportProgress(size_t done, size_t total) {
    // clang-format off
    EM_ASM({ if (Module['_sohExtractProgress']) Module['_sohExtractProgress']($0, $1); }, (double)done, (double)total);
    // clang-format on
}

std::vector<uint8_t> ReadFile(const char* path) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in.is_open()) {
        return {};
    }
    std::vector<uint8_t> bytes(static_cast<size_t>(in.tellg()));
    in.seekg(0);
    in.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
    return bytes;
}

// Leaves the bytes in .z64 order, which is what Torch hashes against config.yml.
Status CheckRom(std::vector<uint8_t>& rom) {
    if (rom.size() < kHeaderBytes) {
        return Status::BadSize;
    }
    // Before the byte swap: a 7z starts with '7' (0x37), which is also the .v64 marker.
    if (RomInfo::LooksCompressed(rom.data(), rom.size())) {
        return Status::Compressed;
    }
    RomInfo::ToBigEndian(rom.data(), rom.size());
    if (!RomInfo::IsValidSize(rom.size())) {
        return Status::BadSize;
    }
    if (RomInfo::TorchVersionDir(RomInfo::HeaderCrc(rom.data(), rom.size())) == nullptr) {
        return Status::UnknownVersion;
    }
    if (!RomInfo::MatchesKnownDump(rom.data(), rom.size())) {
        return Status::BadCrc;
    }
    return Status::Ok;
}

Status ExtractArchive(std::vector<uint8_t> rom, const char* versionDir, const fs::path& outDir,
                      const std::string& expectedArchive, std::string& detail) {
    std::error_code ec;
    fs::create_directories(outDir, ec);
    // Torch skips files whose hashes match a previous run's; start from none.
    fs::remove(outDir / "torch.hash.yml", ec);

    const size_t total = SohTorch::CountAssetFiles(std::string(kRecipeDir) + "/" + versionDir);
    size_t done = 0;
    ReportProgress(0, total);

    const std::string archive = SohTorch::ExtractWithCallbacks(
        std::move(rom), kRecipeDir, outDir.string(), SOH_EXTRACT_PORT_VERSION,
        [&done, total]() { ReportProgress(++done, total); }, [&detail](const std::string& what) { detail = what; });

    fs::remove(outDir / "torch.hash.yml", ec);
    if (archive.empty()) {
        return detail.empty() ? Status::NoArchive : Status::ExtractFailed;
    }
    if (archive != expectedArchive) {
        detail = "Torch wrote " + archive + ", expected " + expectedArchive + ".";
        return Status::NoArchive;
    }
    if (done != total) {
        ReportProgress(total, total);
    }
    return Status::Ok;
}

std::string Quote(const std::string& s) {
    static const char* hex = "0123456789abcdef";
    std::string out = "\"";
    for (unsigned char c : s) {
        if (c == '"' || c == '\\') {
            out += '\\';
            out += static_cast<char>(c);
        } else if (c == '\n') {
            out += "\\n";
        } else if (c < 0x20 || c == 0x7f) {
            out += "\\u00";
            out += hex[c >> 4];
            out += hex[c & 0xf];
        } else {
            out += static_cast<char>(c);
        }
    }
    return out + "\"";
}

int Finish(const Result& result) {
    std::string error = Describe(result.status);
    if (!result.detail.empty()) {
        error += " " + result.detail;
    }
    gLastResultJson = "{\"code\":" + std::to_string(static_cast<int>(result.status)) + ",\"error\":" + Quote(error) +
                      ",\"version\":" + Quote(result.version) + ",\"archive\":" + Quote(result.archive) + "}";
    return static_cast<int>(result.status);
}

} // namespace

extern "C" {

// Removes the ROM file once read so the run does not hold a second copy of it.
int Extract_RomToO2r(const char* romPath, const char* outDir, int quiet) {
    InstallLogger(quiet != 0);
    Result result;

    std::vector<uint8_t> rom = ReadFile(romPath);
    std::error_code ec;
    fs::remove(romPath, ec);
    if (rom.empty()) {
        result.status = Status::CannotRead;
        return Finish(result);
    }
    result.status = CheckRom(rom);
    if (result.status != Status::Ok) {
        return Finish(result);
    }

    const uint32_t headerCrc = RomInfo::HeaderCrc(rom.data(), rom.size());
    result.version = RomInfo::VersionName(headerCrc);
    result.archive = RomInfo::ArchiveName(headerCrc);
    result.status = ExtractArchive(std::move(rom), RomInfo::TorchVersionDir(headerCrc), fs::absolute(outDir),
                                   result.archive, result.detail);
    return Finish(result);
}

const char* Extract_ResultJson() {
    return gLastResultJson.c_str();
}
}
