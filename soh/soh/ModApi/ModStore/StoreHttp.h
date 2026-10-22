#pragma once

#include <cstddef>
#include <string>
#include <vector>

struct StoreHttpResult {
    std::vector<char> body;
    std::string error;
};

bool ModStore_HasHttps();
// Blocking HTTPS GET that follows redirects; call it off the game thread. Fails above maxBytes.
StoreHttpResult ModStore_HttpsGet(const std::string& url, size_t maxBytes);
