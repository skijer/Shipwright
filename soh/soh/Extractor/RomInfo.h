#ifndef ROMINFO_H
#define ROMINFO_H

#include <stddef.h>
#include <stdint.h>

namespace RomInfo {

static constexpr size_t MB_BASE = 1024 * 1024;
static constexpr size_t MB32 = 32 * MB_BASE;
static constexpr size_t MB54 = 54 * MB_BASE;
static constexpr size_t MB64 = 64 * MB_BASE;

void ToBigEndian(uint8_t* rom, size_t romSize);

uint32_t HeaderCrc(const uint8_t* rom, size_t romSize);

bool IsKnownVersion(uint32_t headerCrc);

const char* VersionName(uint32_t headerCrc);

bool IsMasterQuest(uint32_t headerCrc);

const char* TorchVersionDir(uint32_t headerCrc);

const char* ArchiveName(uint32_t headerCrc);

bool IsValidSize(size_t romSize);

bool LooksCompressed(const uint8_t* rom, size_t romSize);

// Some MQ debug dumps have a header patched to look like a US ROM; the CRC is taken as if it were changed back.
bool MatchesKnownDump(uint8_t* rom, size_t romSize);

} // namespace RomInfo

#endif
