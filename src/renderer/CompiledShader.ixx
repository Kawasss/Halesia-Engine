export module Renderer.CompiledShader;

import std;

export struct CompiledShader
{
	std::vector<char> code;
	std::vector<std::uint32_t> externalSets;
};