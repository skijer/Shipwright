#pragma once

#include <iostream>
#include <string>
#include <vector>
#include "StringHelper.h"
#include "VirtualFileSystem.h"

#undef GetCurrentDirectory
#undef CreateDirectory

class Directory
{
public:
	static std::string GetCurrentDirectory() { return ""; }

	static bool Exists(const fs::path& path) { return VirtualFileSystem::HasDirectory(path.string()); }

	static void MakeDirectory(const std::string& path) {}

	static void CreateDirectory(const std::string& path) {}

	static std::vector<std::string> ListFiles(const std::string& dir) { return VirtualFileSystem::ListFiles(dir); }
};
