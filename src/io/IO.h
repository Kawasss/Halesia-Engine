#pragma once
#include <string_view>
#include <vector>
#include <expected>

namespace IO
{
	enum class ReadOptions
	{
		None = 0,
		AddNullTerminator = 1, // only adds a null terminator if the last character is not a null terminator
	};

	extern void WriteFile(const std::string_view& path);
	extern std::expected<std::vector<char>, bool> ReadFile(const std::string_view& path, ReadOptions options = ReadOptions::AddNullTerminator);
}