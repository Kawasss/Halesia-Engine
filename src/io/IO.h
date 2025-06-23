#pragma once
#include <string_view>
#include <vector>

namespace IO
{
	extern void WriteFile(const std::string_view& path);
	extern std::vector<char> ReadFile(const std::string_view& path, bool nullTerm = false);
}