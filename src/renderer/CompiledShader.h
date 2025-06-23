#pragma once
#include <vector>

struct CompiledShader
{
	std::vector<char> code;
	std::vector<uint32_t> externalSets;
};