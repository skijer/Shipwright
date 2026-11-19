#pragma once

#include <cstddef>
#include <string>
#include <vector>

struct StoreHttpResult {
    std::vector<char> body;
    std::string error;
};

bool ModStore_HasHttps();
// Blocking: call it off the game thread.
StoreHttpResult ModStore_HttpsGet(const std::string& url, size_t maxBytes);
