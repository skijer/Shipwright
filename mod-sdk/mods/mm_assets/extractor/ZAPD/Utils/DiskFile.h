#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
#include "Path.h"
#include "Utils/StringHelper.h"
#include "Utils/Directory.h"
#include "VirtualFileSystem.h"

class DiskFile
{
public:
	static bool Exists(const fs::path& filePath) { return VirtualFileSystem::Find(filePath.string()) != nullptr; }

	static std::vector<uint8_t> ReadAllBytes(const fs::path& filePath)
	{
		const std::vector<uint8_t>* data = VirtualFileSystem::Find(filePath.string());
		return data == nullptr ? std::vector<uint8_t>() : *data;
	};

	static std::string ReadAllText(const fs::path& filePath)
	{
		const std::vector<uint8_t>* data = VirtualFileSystem::Find(filePath.string());
		return data == nullptr ? std::string() : std::string(data->begin(), data->end());
	};

	static std::vector<std::string> ReadAllLines(const fs::path& filePath)
	{
		return StringHelper::Split(ReadAllText(filePath), "\n");
	};

	static void WriteAllBytes(const fs::path& filePath, const std::vector<uint8_t>& data)
	{
		VirtualFileSystem::Put(filePath.string(), data);
	};

	static void WriteAllBytes(const std::string& filePath, const std::vector<char>& data)
	{
		VirtualFileSystem::Put(filePath, std::vector<uint8_t>(data.begin(), data.end()));
	};

	static void WriteAllBytes(const std::string& filePath, const char* data, int dataSize)
	{
		VirtualFileSystem::Put(filePath, std::vector<uint8_t>(data, data + dataSize));
	};

	static void WriteAllText(const fs::path& filePath, const std::string& text)
	{
		VirtualFileSystem::Put(filePath.string(), std::vector<uint8_t>(text.begin(), text.end()));
	}
};
