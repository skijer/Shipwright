#include "test_require.h"
#include "mods/pak_loader/pak_selection.h"
#include <algorithm>
#include <vector>

struct Model {
    std::string path;
    bool adult;
    bool child;
    bool equipment;
};

static int Resolve(const std::vector<Model>& models, int index, const char* path, bool Model::*category) {
    return PakSelection::ResolveIndex(
        index, path, static_cast<int>(models.size()), [&](int i) { return models[i].path; },
        [&](int i) { return models[i].*category; });
}

int main() {
    std::vector<Model> models = {
        { "Din/POC9.pak", true, false, true },
        { "Saria/Child.zobj", false, true, true },
        { "equipment/ThicRuto.pak", false, false, true },
    };
    REQUIRE(Resolve(models, 0, nullptr, &Model::adult) == 0);     // Legacy migration.
    REQUIRE(Resolve(models, 0, nullptr, &Model::child) == -1);    // Wrong-age legacy index.
    REQUIRE(Resolve(models, 0, nullptr, &Model::equipment) == 0); // Combined body/equipment donor.
    for (int invalid : { -2, 3, 59, 999999 }) {
        REQUIRE(Resolve(models, invalid, nullptr, &Model::adult) == -1);
    }
    REQUIRE(Resolve(models, -1, nullptr, &Model::child) == -1);

    // A new adult PAK takes the child's old index. The path must still select Saria.
    models.insert(models.begin() + 1, { "Malon/Adult.pak", true, false, true });
    REQUIRE(Resolve(models, 1, "Saria/Child.zobj", &Model::child) == 2);
    REQUIRE(Resolve(models, 2, "equipment/ThicRuto.pak", &Model::equipment) == 3);
    models.erase(models.begin());
    REQUIRE(Resolve(models, 2, "Saria/Child.zobj", &Model::child) == 1);
    std::reverse(models.begin(), models.end());
    REQUIRE(Resolve(models, 0, "Malon/Adult.pak", &Model::adult) == 2);

    // Never fall back to an unrelated, compatible model after deletion or replacement.
    REQUIRE(Resolve(models, 2, "Din/POC9.pak", &Model::adult) == -1);
    REQUIRE(Resolve(models, 2, "Saria/Child.zobj", &Model::adult) == -1);
    REQUIRE(Resolve(models, 2, "", &Model::adult) == -1); // Explicit Default survives stale numeric value.
    REQUIRE(Resolve({}, 0, "Din/POC9.pak", &Model::adult) == -1);

    // Equal filenames in separate folders are independent; moving the app keeps the ID.
    models = { { "a/Model.pak", true, false, true }, { "b/Model.pak", true, false, true } };
    REQUIRE(Resolve(models, 0, "b/Model.pak", &Model::adult) == 1);
    REQUIRE(PakSelection::RelativePath("/old/SoH/mods/a/../b/Model.pak", "/old/SoH/mods") ==
            PakSelection::RelativePath("/new/SoH/mods/b/Model.pak", "/new/SoH/mods"));
    REQUIRE(PakSelection::RelativePath("/SoH/harpoon/skins/Saria.pak", "/SoH/mods") == "../harpoon/skins/Saria.pak");
    puts("PASS PAK selections: add/remove/reorder, age, equipment, legacy and path identity");
}
