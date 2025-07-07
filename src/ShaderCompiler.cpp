#include <fstream>
#include <filesystem>
#include <format>

#include "renderer/ShaderCompiler.h"
#include "renderer/Renderer.h"

#include "io/IO.h"

#include "system/System.h"

constexpr std::string_view BASE_SPIRV_DIRECTORY = "shaders/spirv/"; // the location where all compiled shaders are stored / cached
constexpr std::string_view SPIRV_FILE = ".spv";

namespace fs = std::filesystem;

static bool StringStartsWith(const std::string& str, const std::string_view& cmp)
{
	if (str.size() < cmp.size())
		return false;

	for (int i = 0; i < cmp.size(); i++)
	{
		if (str[i] != cmp[i])
			return false;
	}
	return true;
}

static bool StringIsValidInteger(const std::string_view & str)
{
	for (char ch : str)
	{
		if (!std::isdigit(ch))
			return false;
	}
	return true;
}

std::expected<CompiledShader, bool> ShaderCompiler::Compile(const std::string_view& file)
{
	if (!fs::exists(file))
		return std::unexpected(false); // hand the error over to the caller

	CreateSpirvFile(file);

	CompiledShader ret{};
	ret.externalSets = GetExternalSetsFromSource(file);
	ret.code = ReadSpirvFromSource(file);

	return ret;
}

void ShaderCompiler::CreateSpirvFile(const std::filesystem::path& shader)
{
	if (SpirvIsOutdated(shader))
		CallCompiler(shader);
}

std::vector<uint32_t> ShaderCompiler::GetExternalSetsFromSource(const fs::path& file)
{
	std::vector<uint32_t> ret;
	std::ifstream stream(file);
	while (true)
	{
		if (stream.eof())
			break;

		std::string line;
		stream >> line;

		if (!StringStartsWith(line, "DECLARE_EXTERNAL_SET(") || line.back() != ')') // "DECLARE_EXTERNAL_SET(...)" must be the only thing in that line
			continue;

		size_t begin = line.find('(') + 1;
		size_t end   = line.find(')');

		if (begin >= end)
			continue;

		std::string indexString = line.substr(begin, end - begin);

		if (StringIsValidInteger(indexString))
			ret.push_back(std::stoi(indexString));
	}
	return ret;
}

void ShaderCompiler::CallCompiler(const fs::path& file)
{
	std::string compiler = sys::GetEnvVariable("VK_SDK_PATH") + "\\Bin\\glslc.exe"; // it'll be better if the compiler is bundled in
	
	if (!fs::exists(compiler))
		return;

	std::string args = std::format("-i {} -o shaders/spirv/{}.spv -Dbindless_texture_size={} -Dmaterial_buffer_binding={} -DDECLARE_EXTERNAL_SET(index) --target-env=vulkan1.4", file.string(), file.filename().string(), Renderer::MAX_BINDLESS_TEXTURES, Renderer::MATERIAL_BUFFER_BINDING); // can compile with -O for optimisations
	sys::StartProcess(compiler, args);
}

bool ShaderCompiler::SpirvIsOutdated(const fs::path& src)
{
	fs::path spv = SourcePathToSpirvPath(src);
	
	if (!fs::exists(spv))
		return true;

	return fs::last_write_time(src) > fs::last_write_time(spv);
}

fs::path ShaderCompiler::SourcePathToSpirvPath(const fs::path& file)
{
	return std::format("{}{}{}", BASE_SPIRV_DIRECTORY, file.filename().string(), SPIRV_FILE);
}

std::vector<char> ShaderCompiler::ReadSpirvFromSource(const fs::path& file)
{
	fs::path spvFile = SourcePathToSpirvPath(file);
	return *IO::ReadFile(spvFile.string()); // the file is garantueed to exist at this point so dont check if the file could be read
}