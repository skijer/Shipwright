#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace SharedAssetArchive {

struct ArchiveFile {
    std::string path;
    std::vector<uint8_t> data;
};

struct SongTint {
    const char* song;
    uint8_t red;
    uint8_t green;
    uint8_t blue;
};

constexpr size_t kResourceHeaderSize = 0x40;
constexpr size_t kTextureLayoutSize = 16;
constexpr uint32_t kTextureRgba32 = 1;
constexpr uint32_t kTextureIa8 = 8;
constexpr uint32_t kNoteWidth = 16;
constexpr uint32_t kNoteHeight = 24;
constexpr uint32_t kIconSize = 32;

inline uint32_t ReadU32(const std::vector<uint8_t>& bytes, size_t offset) {
    const uint32_t highHalf = bytes[offset + 2] | static_cast<uint32_t>(bytes[offset + 3]) << 8;
    return bytes[offset] | bytes[offset + 1] << 8 | highHalf << 16;
}

inline void AppendU16(std::vector<uint8_t>& bytes, uint32_t value) {
    bytes.push_back(static_cast<uint8_t>(value));
    bytes.push_back(static_cast<uint8_t>(value >> 8));
}

inline void AppendU32(std::vector<uint8_t>& bytes, uint32_t value) {
    AppendU16(bytes, value & 0xFFFF);
    AppendU16(bytes, value >> 16);
}

inline uint32_t CalculateCrc32(const std::vector<uint8_t>& bytes) {
    uint32_t crc = 0xFFFFFFFF;
    for (uint8_t byte : bytes) {
        crc ^= byte;
        for (int bit = 0; bit < 8; bit++) {
            crc = (crc >> 1) ^ (0xEDB88320 & (0 - (crc & 1)));
        }
    }
    return ~crc;
}

inline bool IsSongNote(const std::vector<uint8_t>& note) {
    const size_t texelCount = kNoteWidth * kNoteHeight;
    return note.size() >= kResourceHeaderSize + kTextureLayoutSize + texelCount &&
           ReadU32(note, kResourceHeaderSize) == kTextureIa8 && ReadU32(note, kResourceHeaderSize + 4) == kNoteWidth &&
           ReadU32(note, kResourceHeaderSize + 8) == kNoteHeight;
}

// Assumes IsSongNote(note).
inline std::vector<uint8_t> MakeTintedIcon(const std::vector<uint8_t>& note, const SongTint& tint) {
    std::vector<uint8_t> icon(note.begin(), note.begin() + kResourceHeaderSize);
    AppendU32(icon, kTextureRgba32);
    AppendU32(icon, kIconSize);
    AppendU32(icon, kIconSize);
    AppendU32(icon, kIconSize * kIconSize * 4);
    const size_t firstPixel = icon.size();
    icon.resize(firstPixel + kIconSize * kIconSize * 4);

    const uint32_t left = (kIconSize - kNoteWidth) / 2;
    const uint32_t top = (kIconSize - kNoteHeight) / 2;
    const uint8_t* texels = note.data() + kResourceHeaderSize + kTextureLayoutSize;
    for (uint32_t y = 0; y < kNoteHeight; y++) {
        for (uint32_t x = 0; x < kNoteWidth; x++) {
            const uint8_t texel = texels[y * kNoteWidth + x];
            const uint32_t intensity = (texel >> 4) * 17;
            uint8_t* pixel = &icon[firstPixel + ((top + y) * kIconSize + left + x) * 4];
            pixel[0] = static_cast<uint8_t>(tint.red * intensity / 255);
            pixel[1] = static_cast<uint8_t>(tint.green * intensity / 255);
            pixel[2] = static_cast<uint8_t>(tint.blue * intensity / 255);
            pixel[3] = static_cast<uint8_t>((texel & 15) * 17);
        }
    }
    return icon;
}

inline std::vector<ArchiveFile> MakeSongNoteIcons(const std::vector<uint8_t>& note, const std::string& prefix,
                                                  const std::vector<SongTint>& tints) {
    std::vector<ArchiveFile> icons;
    if (!IsSongNote(note)) {
        return icons;
    }
    for (const SongTint& tint : tints) {
        icons.push_back({ prefix + tint.song + "Tex", MakeTintedIcon(note, tint) });
    }
    return icons;
}

inline void AppendZipEntryFields(std::vector<uint8_t>& zip, uint32_t crc, uint32_t size, size_t pathLength) {
    constexpr uint32_t kDosDateOf1980 = 0x21;
    AppendU16(zip, 20);
    AppendU16(zip, 0);
    AppendU16(zip, 0);
    AppendU16(zip, 0);
    AppendU16(zip, kDosDateOf1980);
    AppendU32(zip, crc);
    AppendU32(zip, size);
    AppendU32(zip, size);
    AppendU16(zip, static_cast<uint32_t>(pathLength));
    AppendU16(zip, 0);
}

inline std::vector<uint8_t> BuildStoredZip(const std::vector<ArchiveFile>& files) {
    std::vector<uint8_t> zip;
    std::vector<uint8_t> directory;
    for (const ArchiveFile& file : files) {
        const uint32_t crc = CalculateCrc32(file.data);
        const uint32_t size = static_cast<uint32_t>(file.data.size());
        const uint32_t localOffset = static_cast<uint32_t>(zip.size());

        AppendU32(zip, 0x04034B50);
        AppendZipEntryFields(zip, crc, size, file.path.size());
        zip.insert(zip.end(), file.path.begin(), file.path.end());
        zip.insert(zip.end(), file.data.begin(), file.data.end());

        AppendU32(directory, 0x02014B50);
        AppendU16(directory, 20);
        AppendZipEntryFields(directory, crc, size, file.path.size());
        AppendU16(directory, 0);
        AppendU16(directory, 0);
        AppendU16(directory, 0);
        AppendU32(directory, 0);
        AppendU32(directory, localOffset);
        directory.insert(directory.end(), file.path.begin(), file.path.end());
    }
    const uint32_t directoryOffset = static_cast<uint32_t>(zip.size());
    zip.insert(zip.end(), directory.begin(), directory.end());
    AppendU32(zip, 0x06054B50);
    AppendU16(zip, 0);
    AppendU16(zip, 0);
    AppendU16(zip, static_cast<uint32_t>(files.size()));
    AppendU16(zip, static_cast<uint32_t>(files.size()));
    AppendU32(zip, static_cast<uint32_t>(directory.size()));
    AppendU32(zip, directoryOffset);
    AppendU16(zip, 0);
    return zip;
}

} // namespace SharedAssetArchive
