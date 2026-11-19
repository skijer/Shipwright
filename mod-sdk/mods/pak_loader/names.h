#pragma once
#include "catalog.h"
#include <cstring>
#include <string>

namespace pak {
struct PackName {
    std::string title;
    std::string author;
    bool cataloged = false;
};

static std::string Lowercase(std::string text) {
    for (char& c : text) {
        if (c >= 'A' && c <= 'Z') {
            c = char(c + 32);
        }
    }
    return text;
}

static std::string FileName(const std::string& path) {
    size_t slash = path.find_last_of("/\\");
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

static bool HasSuffix(const std::string& text, const std::string& suffix) {
    return text.size() >= suffix.size() && text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

static std::string PrettyName(std::string stem) {
    size_t dot = stem.rfind('.');
    if (dot != std::string::npos) {
        stem.resize(dot);
    }
    static const char* const kPrefixes[] = { "adult_", "child_", "combined_", "zzplayas_", "n64_" };
    static const char* const kSuffixes[] = { "_zzplayas", "_combined", "_standard", "ooto", "_oot", "_mm" };
    for (bool changed = true; changed;) {
        changed = false;
        std::string lower = Lowercase(stem);
        for (const char* prefix : kPrefixes) {
            if (lower.rfind(prefix, 0) == 0) {
                stem.erase(0, strlen(prefix));
                changed = true;
                break;
            }
        }
        for (const char* suffix : kSuffixes) {
            if (!changed && HasSuffix(lower, suffix) && lower.size() > strlen(suffix)) {
                stem.resize(stem.size() - strlen(suffix));
                changed = true;
            }
        }
    }
    std::string pretty;
    for (char c : stem) {
        char spaced = (c == '_' || c == '-') ? ' ' : c;
        if (spaced != ' ' || (!pretty.empty() && pretty.back() != ' ')) {
            pretty += spaced;
        }
    }
    while (!pretty.empty() && pretty.back() == ' ') {
        pretty.pop_back();
    }
    return pretty.empty() ? stem : pretty;
}

static std::string JsonField(const std::string& text, const std::string& key) {
    size_t at = text.find('"' + key + '"');
    if (at == std::string::npos) {
        return "";
    }
    at = text.find(':', at + key.size() + 2);
    if (at == std::string::npos) {
        return "";
    }
    at = text.find_first_not_of(" \t\r\n", at + 1);
    if (at == std::string::npos || text[at] != '"') {
        return "";
    }
    std::string value;
    for (++at; at < text.size() && text[at] != '"'; ++at) {
        if (text[at] == '\\' && at + 1 < text.size()) {
            ++at;
        }
        value += text[at];
    }
    return value;
}

static std::string PlainFileName(const std::string& name) {
    std::string plain = Lowercase(FileName(name));
    for (char& c : plain) {
        bool kept = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';
        c = kept ? c : '_';
    }
    if (!plain.empty() && plain[0] == '.') {
        plain[0] = '_';
    }
    return plain;
}

static const CatalogEntry* FindCatalogEntry(const std::string& fileName) {
    std::string plain = PlainFileName(fileName);
    for (const auto& entry : kCatalog) {
        if (plain == PlainFileName(entry.file)) {
            return &entry;
        }
    }
    return nullptr;
}

static PackName NamePack(const std::string& fileName, const std::string& manifest) {
    if (const CatalogEntry* entry = FindCatalogEntry(fileName)) {
        return { entry->name, entry->author, true };
    }
    std::string declared = JsonField(manifest, "name");
    return { PrettyName(declared.empty() ? FileName(fileName) : declared), JsonField(manifest, "author") };
}

static const char* BodyTag(bool child, bool mm) {
    return mm ? "MM" : child ? "OoT Child" : "OoT Adult";
}
} // namespace pak
