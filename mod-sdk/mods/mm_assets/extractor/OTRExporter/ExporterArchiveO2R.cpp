#include "ExporterArchiveO2R.h"

#include <array>
#include <cstdio>
#include <mutex>
#include <utility>

#include "Utils/VirtualFileSystem.h"

namespace {

constexpr uint32_t kLocalHeaderSignature = 0x04034B50;
constexpr uint32_t kCentralHeaderSignature = 0x02014B50;
constexpr uint32_t kEndOfCentralDirectorySignature = 0x06054B50;
constexpr uint16_t kVersion = 20;
constexpr uint16_t kUtf8NamesFlag = 1 << 11;
constexpr size_t kMaxEntries = 0xFFFF;
constexpr uint64_t kMaxArchiveSize = 0xFFFFFFFF;

const std::array<uint32_t, 256>& Crc32Table() {
    static const std::array<uint32_t, 256> table = [] {
        std::array<uint32_t, 256> entries = {};
        for (uint32_t index = 0; index < 256; index++) {
            uint32_t value = index;
            for (int bit = 0; bit < 8; bit++) {
                value = (value & 1) ? (0xEDB88320 ^ (value >> 1)) : (value >> 1);
            }
            entries[index] = value;
        }
        return entries;
    }();
    return table;
}

uint32_t Crc32(const std::vector<uint8_t>& data) {
    uint32_t crc = 0xFFFFFFFF;
    for (uint8_t byte : data) {
        crc = Crc32Table()[(crc ^ byte) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFF;
}

void Write16(std::vector<uint8_t>& out, uint16_t value) {
    out.push_back(static_cast<uint8_t>(value));
    out.push_back(static_cast<uint8_t>(value >> 8));
}

void Write32(std::vector<uint8_t>& out, uint32_t value) {
    Write16(out, static_cast<uint16_t>(value));
    Write16(out, static_cast<uint16_t>(value >> 16));
}

void WriteName(std::vector<uint8_t>& out, const std::string& name) {
    out.insert(out.end(), name.begin(), name.end());
}

} // namespace

ExporterArchiveO2R::ExporterArchiveO2R(const std::string& path, bool enableWriting) {
    mPath = path;
}

ExporterArchiveO2R::~ExporterArchiveO2R() {
    Unload();
}

bool ExporterArchiveO2R::Load(bool enableWriting) {
    printf("Appending to an existing archive is not supported: '%s' starts empty\n", mPath.c_str());
    return CreateArchive(0) == 0;
}

int ExporterArchiveO2R::CreateArchive([[maybe_unused]] size_t fileCapacity) {
    const std::lock_guard<std::mutex> lock(mMutex);
    mEntries.clear();
    mEntryIndices.clear();
    mOpen = true;
    return 0;
}

bool ExporterArchiveO2R::AddFile(const std::string& filePath, void* fileData, size_t fileSize) {
    const std::lock_guard<std::mutex> lock(mMutex);
    const uint8_t* bytes = static_cast<const uint8_t*>(fileData);
    std::vector<uint8_t> data(bytes, bytes + fileSize);

    auto existing = mEntryIndices.find(filePath);
    if (existing != mEntryIndices.end()) {
        mEntries[existing->second].data = std::move(data);
        return true;
    }
    if (mEntries.size() >= kMaxEntries) {
        printf("Archive '%s' is full: a stored ZIP holds at most %zu entries\n", mPath.c_str(), kMaxEntries);
        return false;
    }
    mEntryIndices.emplace(filePath, mEntries.size());
    mEntries.push_back({ filePath, std::move(data) });
    return true;
}

bool ExporterArchiveO2R::Unload() {
    const std::lock_guard<std::mutex> lock(mMutex);
    if (!mOpen) {
        return true;
    }
    mOpen = false;

    std::vector<uint8_t> zip = BuildZip();
    if (zip.empty()) {
        printf("Archive '%s' is larger than a ZIP without ZIP64 allows\n", mPath.c_str());
        return false;
    }
    VirtualFileSystem::Put(mPath, std::move(zip));
    mEntries.clear();
    mEntryIndices.clear();
    return true;
}

std::vector<uint8_t> ExporterArchiveO2R::BuildZip() const {
    std::vector<uint8_t> zip;
    std::vector<uint8_t> centralDirectory;

    for (const Entry& entry : mEntries) {
        const uint64_t localHeaderOffset = zip.size();
        if (localHeaderOffset + entry.data.size() > kMaxArchiveSize) {
            return {};
        }
        const uint32_t crc = Crc32(entry.data);
        const uint32_t size = static_cast<uint32_t>(entry.data.size());
        const uint16_t nameLength = static_cast<uint16_t>(entry.name.size());

        Write32(zip, kLocalHeaderSignature);
        Write16(zip, kVersion);
        Write16(zip, kUtf8NamesFlag);
        Write16(zip, 0);
        Write16(zip, 0);
        Write16(zip, 0);
        Write32(zip, crc);
        Write32(zip, size);
        Write32(zip, size);
        Write16(zip, nameLength);
        Write16(zip, 0);
        WriteName(zip, entry.name);
        zip.insert(zip.end(), entry.data.begin(), entry.data.end());

        Write32(centralDirectory, kCentralHeaderSignature);
        Write16(centralDirectory, kVersion);
        Write16(centralDirectory, kVersion);
        Write16(centralDirectory, kUtf8NamesFlag);
        Write16(centralDirectory, 0);
        Write16(centralDirectory, 0);
        Write16(centralDirectory, 0);
        Write32(centralDirectory, crc);
        Write32(centralDirectory, size);
        Write32(centralDirectory, size);
        Write16(centralDirectory, nameLength);
        Write16(centralDirectory, 0);
        Write16(centralDirectory, 0);
        Write16(centralDirectory, 0);
        Write16(centralDirectory, 0);
        Write32(centralDirectory, 0);
        Write32(centralDirectory, static_cast<uint32_t>(localHeaderOffset));
        WriteName(centralDirectory, entry.name);
    }

    const uint64_t centralDirectoryOffset = zip.size();
    if (centralDirectoryOffset + centralDirectory.size() > kMaxArchiveSize) {
        return {};
    }
    zip.insert(zip.end(), centralDirectory.begin(), centralDirectory.end());

    const uint16_t entryCount = static_cast<uint16_t>(mEntries.size());
    Write32(zip, kEndOfCentralDirectorySignature);
    Write16(zip, 0);
    Write16(zip, 0);
    Write16(zip, entryCount);
    Write16(zip, entryCount);
    Write32(zip, static_cast<uint32_t>(centralDirectory.size()));
    Write32(zip, static_cast<uint32_t>(centralDirectoryOffset));
    Write16(zip, 0);
    return zip;
}
