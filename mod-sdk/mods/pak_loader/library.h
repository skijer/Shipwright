#pragma once
#include "emotes.h"
#include "equipment.h"
#include "names.h"
#include <algorithm>
#include <cstdio>
#include <functional>
#include <memory>

namespace pak {
void Log(const Api* api, const char* message);
bool CanReadModsFolder(const Api* api);

static std::string Hex(uint32_t value) {
    char text[9];
    snprintf(text, sizeof(text), "%08x", value);
    return text;
}

template <typename Item> static void SortByLabel(std::vector<std::unique_ptr<Item>>& items) {
    std::stable_sort(items.begin(), items.end(),
                     [](const auto& a, const auto& b) { return Lowercase(a->label) < Lowercase(b->label); });
}

template <typename Item>
static Item* FindByKey(const std::vector<std::unique_ptr<Item>>& items, const std::string& key) {
    for (const auto& item : items) {
        if (item->key == key) {
            return item.get();
        }
    }
    return nullptr;
}

class Library {
  public:
    std::vector<std::unique_ptr<Model>> models;
    std::vector<std::unique_ptr<Piece>> pieces;
    std::vector<std::unique_ptr<VoicePack>> voices;
    std::vector<std::unique_ptr<Emote>> emotes;
    std::string status = "Choose a model for each appearance. Abilities stay with the current form.";
    bool restartNeeded = false;
    bool scanned = false;

    void Bind(const Api* modApi) {
        api = modApi;
    }

    Model* Find(const std::string& key) const {
        return FindByKey(models, key);
    }

    Piece* FindPiece(const std::string& key) const {
        return key.empty() ? nullptr : FindByKey(pieces, key);
    }

    VoicePack* FindVoice(const std::string& key) const {
        return key.empty() ? nullptr : FindByKey(voices, key);
    }

    Emote* FindEmote(const std::string& key) const {
        return key.empty() ? nullptr : FindByKey(emotes, key);
    }

    void Scan() {
        api->ListArchiveFiles("*Skel*", AddNativeCallback, this);
        api->ReadArchiveFiles("pak_loader/imports/*", AddStoredCallback, this);
        if (CanReadModsFolder(api)) {
            api->ReadModsFolderFiles(".pak;.zobj", AddStoredCallback, this);
        }
        std::stable_sort(models.begin(), models.end(), [](const auto& a, const auto& b) {
            if (a->native != b->native) {
                return a->native;
            }
            return !a->native && Lowercase(a->label) < Lowercase(b->label);
        });
        SortByLabel(pieces);
        SortByLabel(voices);
        SortByLabel(emotes);
        scanned = true;
        size_t builtIn = std::count_if(models.begin(), models.end(), [](const auto& model) { return model->native; });
        status = std::to_string(builtIn) + " built-in and " + std::to_string(models.size() - builtIn) +
                 " imported models, " + std::to_string(pieces.size()) + " equipment pieces, " +
                 std::to_string(voices.size()) + " voices and " + std::to_string(emotes.size()) + " emotes.";
    }

    Model* Load(const std::string& key) {
        Model* model = Find(key);
        if (model == nullptr || model->ready) {
            return model;
        }
        try {
            model->LoadNative();
            return model;
        } catch (const std::exception& error) {
            status = model->label + ": " + error.what();
            Log(api, status.c_str());
            return nullptr;
        }
    }

    static std::string Describe(const std::string& label, const std::string& author) {
        return author.empty() ? label : label + " by " + author;
    }

    void PickAndImport() {
        UserFile file{};
        file.structSize = sizeof(file);
        if (!api->PickUserFile("Import a ModLoader64 pack", "ModLoader64 packs", "*.pak *.zobj", &file)) {
            return;
        }
        try {
            Require(file.size <= Limit, "Pack exceeds 32 MiB");
            std::string name = FileName(file.name);
            std::string summary = Import(name, file.data, size_t(file.size));
            bool saved = SaveCopy(name, file.data, size_t(file.size));
            restartNeeded |= saved;
            status = "Imported " + summary + ". " +
                     (saved ? "Saved. Restart to update the lists."
                            : "Not saved. Import again and allow saving to add it to the lists.");
        } catch (const std::exception& error) { status = error.what(); }
        api->FreeUserFile(&file);
    }

    void InstallArchive() {
        UserFile file{};
        file.structSize = sizeof(file);
        if (!api->PickUserFile("Install a player model archive", "Ship archives", "*.o2r *.otr *.zip", &file)) {
            return;
        }
        std::string extension = HasSuffix(Lowercase(file.name), ".otr") ? ".otr" : ".o2r";
        std::string name = "pakloader_models_" + Hex(Crc(file.data, size_t(file.size))) + extension;
        bool saved = api->SaveToModsFolder(name.c_str(), file.data, file.size);
        api->FreeUserFile(&file);
        restartNeeded |= saved;
        status = saved ? "Archive installed. Restart to update the model lists." : "Archive was not installed.";
    }

  private:
    const Api* api = nullptr;

    bool SaveCopy(const std::string& name, const uint8_t* data, size_t size) {
        if (CanReadModsFolder(api)) {
            return api->SaveToModsFolder(PlainFileName(name).c_str(), data, size);
        }
        std::string hash = Hex(Crc(data, size));
        Bytes archive = Wrap("pak_loader/imports/" + hash + "/" + name, data, size);
        return api->SaveToModsFolder(("pakloader_import_" + hash + ".o2r").c_str(), archive.data(), archive.size());
    }
    std::set<std::string> imported;
    std::vector<std::unique_ptr<Model>> equipmentModels;

    static bool IsOriginalLinkDirectory(const std::string& directory) {
        static const char* const kDirectories[] = { "objects/object_link_boy", "objects/object_link_child",
                                                    "objects/oot_object_link_boy", "objects/oot_object_link_child" };
        for (const char* original : kDirectories) {
            if (directory == original) {
                return true;
            }
        }
        return false;
    }

    static std::string NativeLabel(const Model& model) {
        const char* base = model.deity   ? "MM Fierce Deity"
                           : model.mm    ? "MM Child Link"
                           : model.child ? "OoT Child Link"
                                         : "OoT Adult Link";
        std::string directory = model.prefix.empty() ? "" : model.prefix.substr(0, model.prefix.size() - 1);
        bool alternate = directory.rfind("alt/", 0) == 0;
        if (alternate) {
            directory.erase(0, 4);
        }
        std::string label = IsOriginalLinkDirectory(directory) ? std::string(base)
                                                               : PrettyName(FileName(directory)) + " (" + base + ")";
        return alternate ? label + " (alt assets)" : label;
    }

    void AddNative(const std::string& path) {
        std::string lower = Lowercase(path);
        bool skeletonLeaf = lower.find("link") != std::string::npos && HasSuffix(lower, "skel");
        if (!skeletonLeaf || Find("otr:" + path) != nullptr) {
            return;
        }
        bool deity = lower.find("fiercedeity") != std::string::npos;
        bool human = lower.find("linkhuman") != std::string::npos;
        bool child = human || lower.find("linkchild") != std::string::npos;
        if (!deity && !child && lower.find("linkadult") == std::string::npos) {
            return;
        }
        auto model = std::make_unique<Model>();
        model->key = "otr:" + path;
        model->path = path;
        model->native = true;
        model->child = child;
        model->mm = human || deity;
        model->deity = deity;
        size_t slash = path.rfind('/');
        model->prefix = slash == std::string::npos ? "" : path.substr(0, slash + 1);
        model->stem = FileName(path).substr(0, FileName(path).size() - 4);
        model->label = NativeLabel(*model);
        models.push_back(std::move(model));
    }

    static std::string VariantName(const Model& model) {
        return PrettyName(FileName(model.key.substr(model.key.find(':', 4) + 1)));
    }

    static void LabelPack(const std::string& title, const std::vector<Model*>& pack) {
        std::map<std::string, int> totals;
        std::set<std::string> variants;
        bool distinctVariants = true;
        for (const Model* model : pack) {
            std::string tag = BodyTag(model->child, model->mm);
            ++totals[tag];
            distinctVariants &= variants.insert(tag + VariantName(*model)).second;
        }
        std::map<std::string, int> seen;
        for (Model* model : pack) {
            std::string tag = BodyTag(model->child, model->mm);
            int number = ++seen[tag];
            model->label = title + " (" + tag + ")";
            if (totals[tag] > 1) {
                model->label += distinctVariants ? " - " + VariantName(*model) : " #" + std::to_string(number);
            }
        }
    }

    void AddPieces(const Model& model, const std::function<std::string(const Piece&)>& label) {
        for (Piece piece : CollectPieces(model)) {
            piece.key = model.key + "#" + SlotKey(piece.kind, piece.variant);
            piece.label = label(piece);
            pieces.push_back(std::make_unique<Piece>(piece));
        }
    }

    std::string Import(const std::string& fileName, const uint8_t* data, size_t size) {
        std::string hash = Hex(Crc(data, size));
        if (imported.count(hash)) {
            return "nothing new";
        }
        std::vector<File> files;
        if (HasSuffix(Lowercase(fileName), ".zobj")) {
            files.push_back({ fileName, Bytes(data, data + size) });
        } else {
            files = Read(data, size);
        }
        std::string manifest;
        for (const auto& file : files) {
            if (HasSuffix(Lowercase(file.name), "package.json")) {
                manifest.assign(file.data.begin(), file.data.end());
            }
        }
        PackName name = NamePack(fileName, manifest);
        std::vector<Model*> bodies;
        std::vector<Model*> equipment;
        std::string failure;
        for (auto& file : files) {
            if (!HasSuffix(Lowercase(file.name), ".zobj")) {
                continue;
            }
            auto model = std::make_unique<Model>();
            model->key = "pak:" + hash + ":" + file.name;
            model->author = name.author;
            model->bytes = std::move(file.data);
            try {
                model->LoadZobj();
                bodies.push_back(model.get());
                models.push_back(std::move(model));
                continue;
            } catch (const std::exception& error) { failure = file.name + ": " + error.what(); }
            try {
                model->LoadEquipment();
                equipment.push_back(model.get());
                equipmentModels.push_back(std::move(model));
            } catch (const std::exception&) {}
        }
        LabelPack(name.title, bodies);
        size_t firstPiece = pieces.size();
        for (Model* body : bodies) {
            AddPieces(*body,
                      [&](const Piece& piece) { return body->label + " - " + PieceName(piece.kind, piece.variant); });
        }
        for (Model* item : equipment) {
            AddPieces(*item, [&](const Piece& piece) {
                if (equipment.size() == 1) {
                    return name.title + " (" + PieceName(piece.kind, piece.variant) + ")";
                }
                std::string own = item->equipmentName.empty() ? PieceName(piece.kind, piece.variant)
                                                              : PrettyName(item->equipmentName);
                return name.title + " - " + own;
            });
        }
        bool voice = AddVoice(files, hash, name);
        bool emote = AddEmote(files, hash, name, manifest);
        size_t equipmentPieces = pieces.size() - firstPiece;
        Require(!bodies.empty() || equipmentPieces > 0 || voice || emote,
                failure.empty() ? "No supported body, equipment, voice or emote in this file" : failure.c_str());
        imported.insert(hash);
        std::string summary =
            std::to_string(bodies.size()) + " models, " + std::to_string(equipmentPieces) + " equipment pieces";
        return summary + (voice ? ", a voice" : "") + (emote ? ", an emote" : "");
    }

    bool AddVoice(const std::vector<File>& files, const std::string& hash, const PackName& name) {
        auto pack = std::make_unique<VoicePack>();
        for (const auto& file : files) {
            std::string lower = Lowercase(file.name);
            size_t folder = lower.find("sounds/");
            if (folder == std::string::npos || !HasSuffix(lower, ".ogg")) {
                continue;
            }
            size_t idStart = folder + 7;
            size_t idEnd = lower.find('/', idStart);
            if (idEnd == std::string::npos) {
                continue;
            }
            uint16_t id = uint16_t(strtoul(lower.substr(idStart, idEnd - idStart).c_str(), nullptr, 16));
            if (id != 0) {
                pack->sources[id].push_back(file.data);
            }
        }
        if (pack->sources.empty()) {
            return false;
        }
        pack->key = "voice:" + hash;
        pack->label = name.title;
        pack->author = name.author;
        voices.push_back(std::move(pack));
        return true;
    }

    bool AddEmote(const std::vector<File>& files, const std::string& hash, const PackName& name,
                  const std::string& manifest) {
        size_t object = manifest.find("\"emote\"");
        if (object == std::string::npos) {
            return false;
        }
        std::string fields = manifest.substr(object, manifest.find('}', object) - object);
        std::string anim = JsonField(fields, "anim");
        std::string sound = JsonField(fields, "sound");
        auto emote = std::make_unique<Emote>();
        for (const auto& file : files) {
            if (!anim.empty() && HasSuffix(file.name, anim) && !ReadEmoteFrames(file.data, *emote)) {
                return false;
            }
            if (!sound.empty() && HasSuffix(file.name, sound)) {
                emote->soundSource = file.data;
            }
        }
        if (emote->frameCount == 0) {
            return false;
        }
        size_t repeats = fields.find("\"repeats\"");
        emote->loops = repeats != std::string::npos && fields.find("true", repeats) < fields.find(',', repeats);
        emote->key = "emote:" + hash;
        std::string declared = JsonField(fields, "name");
        emote->label = name.cataloged || declared.empty() ? name.title : declared;
        emote->author = name.author;
        emotes.push_back(std::move(emote));
        return true;
    }

    static bool AddNativeCallback(const char* path, void* self) {
        static_cast<Library*>(self)->AddNative(path);
        return true;
    }

    static bool AddStoredCallback(const char* path, const uint8_t* data, uint64_t size, void* self) {
        auto* library = static_cast<Library*>(self);
        try {
            if (size <= Limit) {
                library->Import(FileName(path), data, size_t(size));
            }
        } catch (const std::exception& error) { library->status = std::string(path) + ": " + error.what(); }
        return true;
    }
};
} // namespace pak
