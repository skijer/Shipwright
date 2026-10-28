#include "soh/ModApi/ModStore/StoreCatalog.h"
#include "soh/ModApi/ModStore/StorePackDefaults.h"
#include <set>
#include <cassert>
#include <iostream>

int main() {
  const auto catalog = ModStore_ParseCatalog(R"({
        "format": 1,
        "mods": {
            "masks": {
                "name": "Masks", "author": "Author and contributors", "owner": "author",
                "builds": {}, "dependency_only": true,
                "options": [
                    {"id": "blast", "name": "Blast Mask", "cvar": "gCheats.InfiniteHealth"},
                    {"id": "blast", "name": "Duplicate"},
                    {"id": "../../outside", "name": "Invalid"},
                    {"id": "empty", "name": ""},
                    {"id": "stone", "name": "Stone Mask"},
                    null
                ]
            },
            "private": {"listed": false, "builds": {}}
        },
        "packs": [
            {"id": "mm-items", "name": "MM Items", "version": "2.4", "mods": ["masks", "masks", "private", "missing"]},
            {"id": "mm-items", "name": "Duplicate pack", "mods": ["masks"]}
        ]
    })");
  assert(catalog);
  const auto &mod = catalog->mods.at("masks");
  assert(mod.owner == "author");
  assert(mod.dependencyOnly);
  assert(mod.options.size() == 2);
  assert(mod.options[0].id == "blast");
  assert(mod.options[1].name == "Stone Mask");
  assert(catalog->packs.size() == 1);
  assert(catalog->packs[0].mods == std::vector<std::string>{"masks"});
  const auto legacy =
      ModStore_ParseCatalog(R"({"format":1,"mods":{"old":{"builds":{}}}})");
  assert(legacy && legacy->mods.at("old").options.empty());
  const auto packs = ModStore_DefaultPacks();
  assert(packs.size() == 8);
  const size_t expectedCounts[] = {3, 28, 20, 3, 9, 5, 6, 3};
  std::set<std::string> members;
  for (size_t i = 0; i < packs.size(); ++i) {
    assert(packs[i].mods.size() == expectedCounts[i]);
    assert(!packs[i].icon.empty());
    for (const auto& id : packs[i].mods) {
      assert(id != "mm_assets");
      assert(members.insert(id).second);
    }
  }
  assert(members.size() == 77);
  assert(std::find(packs[2].mods.begin(), packs[2].mods.end(), "four_sword") != packs[2].mods.end());
  assert(std::find(packs[7].mods.begin(), packs[7].mods.end(), "ray_gun") != packs[7].mods.end());
  std::cout << "Store catalog options and legacy compatibility passed\n";
}
