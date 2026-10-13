#include <ship/utils/StrHash64.h>

#include <array>

// soh.exe does not export libultraship's CRC64, so the exporter hashes resource paths with its own copy:
// ECMA-182, MSB first, no final inversion, the same values the game looks resources up by.
namespace {

constexpr uint64_t kPolynomial = 0x42F0E1EBA9EA3693ULL;

std::array<uint64_t, 256> BuildTable() {
    std::array<uint64_t, 256> table = {};
    for (uint64_t index = 0; index < table.size(); index++) {
        uint64_t crc = index << 56;
        for (int32_t bit = 0; bit < 8; bit++) {
            crc = (crc & (1ULL << 63)) ? (crc << 1) ^ kPolynomial : crc << 1;
        }
        table[index] = crc;
    }
    return table;
}

} // namespace

uint64_t CRC64(const char* text) {
    static const std::array<uint64_t, 256> table = BuildTable();
    uint64_t crc = INITIAL_CRC64;
    for (const unsigned char* at = reinterpret_cast<const unsigned char*>(text); *at != '\0'; at++) {
        crc = table[static_cast<uint8_t>(crc >> 56) ^ *at] ^ (crc << 8);
    }
    return crc;
}
