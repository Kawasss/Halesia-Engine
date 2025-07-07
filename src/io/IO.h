#pragma once
#include <string_view>
#include <vector>
#include <expected>

namespace IO
{
	extern void WriteFile(const std::string_view& path);
	extern std::expected<std::vector<char>, bool> ReadFile(const std::string_view& path, bool nullTerm = false);
}