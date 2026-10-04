#include "../soh/Enhancements/Graphics/NativeMaterialProfile.h"
#include "test_require.h"
#include <fstream>
#include <iostream>

int main(int argc, char** argv) {
    REQUIRE(argc == 2);
    std::ifstream input(argv[1]);
    const auto fixture = nlohmann::json::parse(input);
    nlohmann::json result = nlohmann::json::array();
    for (const auto& item : fixture) {
        auto profile = Prelude::ResolveNativeMaterial(item["metadata"], item["pasted"]);
        if (profile == Prelude::NativeMaterialProfile::None) {
            continue;
        }
        std::vector<Prelude::NativeMaterialCommand> commands;
        for (const auto& pair : item["commands"]) {
            commands.push_back({ pair[0].get<uintptr_t>(), pair[1].get<uintptr_t>() });
        }
        auto insertion = item["ucode"] == 4 ? Prelude::FindNativeScrollInsertion(commands, profile) : std::nullopt;
        result.push_back({ { "path", item["path"] },
                           { "profile", static_cast<int>(profile) },
                           { "insertion", insertion ? nlohmann::json(*insertion) : nlohmann::json(nullptr) } });
    }
    std::cout << result.dump(2) << '\n';
}
