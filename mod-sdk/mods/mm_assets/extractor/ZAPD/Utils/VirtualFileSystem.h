#pragma once

// The mod's ZAPD never touches the disk: every path it opens names an entry in this in-memory store, which
// mm_assets fills with the ROM and the XMLs and empties of the finished archive.

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace fs {

class path {
  public:
    path() = default;
    path(const std::string& text);
    path(const char* text);

    const std::string& string() const;
    const std::string& generic_string() const;
    const char* c_str() const;
    bool empty() const;

    path filename() const;
    path stem() const;
    path extension() const;
    path parent_path() const;

    path& operator/=(const path& child);
    path& concat(const std::string& suffix);
    operator std::string() const;

  private:
    std::string mText;
};

path operator/(const path& parent, const path& child);
bool operator==(const path& left, const path& right);
bool operator!=(const path& left, const path& right);
bool operator<(const path& left, const path& right);

} // namespace fs

class VirtualFileSystem {
  public:
    static std::string Normalize(const std::string& path);

    static void Clear();
    static void Put(const std::string& path, std::vector<uint8_t> data);
    static const std::vector<uint8_t>* Find(const std::string& path);
    static std::vector<uint8_t> Take(const std::string& path);
    static bool HasDirectory(const std::string& directory);
    static std::vector<std::string> ListFiles(const std::string& directory);

  private:
    static std::map<std::string, std::vector<uint8_t>>& Files();
};
