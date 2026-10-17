#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include "ExporterArchive.h"

// A stored (uncompressed) ZIP built in memory: the finished archive lands in the extractor's virtual file system,
// and libultraship reads stored entries like any other.
class ExporterArchiveO2R : public ExporterArchive {
  public:
    ExporterArchiveO2R(const std::string& path, bool enableWriting);
    ~ExporterArchiveO2R();

    int CreateArchive(size_t fileCapacity) override;
    bool AddFile(const std::string& filePath, void* fileData, size_t fileSize) override;

    bool Load(bool enableWriting) override;
    bool Unload() override;

  private:
    struct Entry {
        std::string name;
        std::vector<uint8_t> data;
    };

    std::vector<uint8_t> BuildZip() const;

    std::vector<Entry> mEntries;
    std::unordered_map<std::string, size_t> mEntryIndices;
    bool mOpen = false;
};
