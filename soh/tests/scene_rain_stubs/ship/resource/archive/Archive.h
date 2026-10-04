#pragma once
#include <map>
#include <memory>
#include <string>
#include <vector>
namespace Ship {
struct File {
    std::shared_ptr<std::vector<char>> Buffer;
};
class Archive {
  public:
    std::string path;
    std::map<std::string, std::string> files;
    int reads = 0;
    explicit Archive(std::string name) : path(name) {
    }
    bool HasFile(const std::string& file) {
        return files.count(file);
    }
    const std::string& GetPath() {
        return path;
    }
    std::shared_ptr<File> LoadFile(const std::string& file) {
        ++reads;
        if (!HasFile(file))
            return nullptr;
        auto result = std::make_shared<File>();
        result->Buffer = std::make_shared<std::vector<char>>(files[file].begin(), files[file].end());
        return result;
    }
};
struct ResourceInitData {
    std::shared_ptr<Archive> Parent;
    std::string Path;
};
} // namespace Ship
