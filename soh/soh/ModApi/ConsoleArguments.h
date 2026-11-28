#pragma once

#include <string>

inline std::string ModApi_UnquoteConsoleArgument(const std::string& argument) {
    if (argument.size() >= 2 && argument.front() == '"' && argument.back() == '"') {
        return argument.substr(1, argument.size() - 2);
    }
    return argument;
}
