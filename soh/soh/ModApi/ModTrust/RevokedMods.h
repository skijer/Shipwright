#pragma once

#include <initializer_list>
#include <string_view>

// The sign-mod workflow also reads these: a listed author can no longer get anything signed.
// One lowercase GitHub login per line.
inline constexpr std::initializer_list<std::string_view> kRevokedModAuthors = {};

// One package content hash (the "content_hash" of its unbound-signature.json) per line.
inline constexpr std::initializer_list<std::string_view> kRevokedModPackages = {};
