#include "VirtualFileSystem.h"

#include <utility>

namespace fs {

path::path(const std::string& text) : mText(VirtualFileSystem::Normalize(text)) {
}

path::path(const char* text) : path(std::string(text == nullptr ? "" : text)) {
}

const std::string& path::string() const {
    return mText;
}

const std::string& path::generic_string() const {
    return mText;
}

const char* path::c_str() const {
    return mText.c_str();
}

bool path::empty() const {
    return mText.empty();
}

path path::filename() const {
    size_t slash = mText.find_last_of('/');
    return slash == std::string::npos ? *this : path(mText.substr(slash + 1));
}

path path::stem() const {
    std::string name = filename().string();
    size_t dot = name.find_last_of('.');
    return (dot == std::string::npos || dot == 0) ? path(name) : path(name.substr(0, dot));
}

path path::extension() const {
    std::string name = filename().string();
    size_t dot = name.find_last_of('.');
    return (dot == std::string::npos || dot == 0) ? path() : path(name.substr(dot));
}

path path::parent_path() const {
    size_t slash = mText.find_last_of('/');
    return slash == std::string::npos ? path() : path(mText.substr(0, slash));
}

path& path::operator/=(const path& child) {
    if (child.mText.empty()) {
        return *this;
    }
    mText = mText.empty() ? child.mText : VirtualFileSystem::Normalize(mText + "/" + child.mText);
    return *this;
}

path& path::concat(const std::string& suffix) {
    mText = VirtualFileSystem::Normalize(mText + suffix);
    return *this;
}

path::operator std::string() const {
    return mText;
}

path operator/(const path& parent, const path& child) {
    path joined = parent;
    joined /= child;
    return joined;
}

bool operator==(const path& left, const path& right) {
    return left.string() == right.string();
}

bool operator!=(const path& left, const path& right) {
    return !(left == right);
}

bool operator<(const path& left, const path& right) {
    return left.string() < right.string();
}

} // namespace fs

std::string VirtualFileSystem::Normalize(const std::string& path) {
    std::vector<std::string> parts;
    std::string part;
    auto flush = [&]() {
        if (part == "..") {
            if (!parts.empty()) {
                parts.pop_back();
            }
        } else if (!part.empty() && part != ".") {
            parts.push_back(part);
        }
        part.clear();
    };
    for (char character : path) {
        if (character == '/' || character == '\\') {
            flush();
        } else {
            part += character;
        }
    }
    flush();

    std::string normalized;
    for (const std::string& segment : parts) {
        if (!normalized.empty()) {
            normalized += '/';
        }
        normalized += segment;
    }
    return normalized;
}

std::map<std::string, std::vector<uint8_t>>& VirtualFileSystem::Files() {
    static std::map<std::string, std::vector<uint8_t>> files;
    return files;
}

void VirtualFileSystem::Clear() {
    Files().clear();
}

void VirtualFileSystem::Put(const std::string& path, std::vector<uint8_t> data) {
    Files()[Normalize(path)] = std::move(data);
}

const std::vector<uint8_t>* VirtualFileSystem::Find(const std::string& path) {
    auto found = Files().find(Normalize(path));
    return found == Files().end() ? nullptr : &found->second;
}

std::vector<uint8_t> VirtualFileSystem::Take(const std::string& path) {
    auto found = Files().find(Normalize(path));
    if (found == Files().end()) {
        return {};
    }
    std::vector<uint8_t> data = std::move(found->second);
    Files().erase(found);
    return data;
}

bool VirtualFileSystem::HasDirectory(const std::string& directory) {
    std::string prefix = Normalize(directory);
    if (prefix.empty()) {
        return true;
    }
    prefix += '/';
    auto candidate = Files().lower_bound(prefix);
    return candidate != Files().end() && candidate->first.compare(0, prefix.size(), prefix) == 0;
}

std::vector<std::string> VirtualFileSystem::ListFiles(const std::string& directory) {
    std::string prefix = Normalize(directory);
    if (!prefix.empty()) {
        prefix += '/';
    }
    std::vector<std::string> files;
    for (auto entry = Files().lower_bound(prefix); entry != Files().end(); ++entry) {
        if (entry->first.compare(0, prefix.size(), prefix) != 0) {
            break;
        }
        files.push_back(entry->first);
    }
    return files;
}
