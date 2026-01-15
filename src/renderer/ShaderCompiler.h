#pragma once
#include <vector>
#include <string_view>
#include <string>
#include <filesystem>
#include <expected>

import Renderer.CompiledShader;

class ShaderCompiler
{
public:
	static std::expected<CompiledShader, bool> Compile(const std::string_view& file);

private:
	static std::vector<uint32_t> GetExternalSetsFromSource(const std::filesystem::path& file);
	static void CallCompiler(const std::filesystem::path& shader);
	static void CreateSpirvFile(const std::filesystem::path& shader);

	static bool SpirvIsOutdated(const std::filesystem::path& src); // src refers to the glsl file, it automatically generates the path to the spirv file
	static std::filesystem::path SourcePathToSpirvPath(const std::filesystem::path& file);

	static std::vector<char> ReadSpirvFromSource(const std::filesystem::path& file); // reads the compiled spirv from the source file
};