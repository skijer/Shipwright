#pragma once

#include <iostream>
#include <string>
#include "Utils/StringHelper.h"
#include "VirtualFileSystem.h"

class Path
{
public:
	static std::string GetFileName(const fs::path& input)
	{
		return input.filename().string();
	};

	static std::string GetFileNameWithoutExtension(const fs::path& input)
	{
		return input.stem().string();
	};

	static std::string GetFileNameExtension(const std::string& input)
	{
		return input.substr(input.find_last_of("."), input.length());
	};

	static fs::path GetPath(const std::string& input)
	{
		std::vector<std::string> split = StringHelper::Split(input, "/");
		fs::path output;

		for (std::string str : split)
		{
			if (str.find_last_of(".") == std::string::npos)
				output /= str;
		}

		return output;
	};

	static fs::path GetDirectoryName(const fs::path& path) { return path.parent_path(); };
};
