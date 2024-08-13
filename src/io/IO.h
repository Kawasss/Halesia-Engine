#pragma once
#include <string>
#include <vector>

namespace IO
{
	extern void WriteFile(const std::string& path);
	extern std::vector<char> ReadFile(const std::string& path, bool nullTerm = false);
}