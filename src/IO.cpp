#include <fstream>

#include "io/IO.h"

namespace IO
{
	void WriteFile(const std::string_view& path)
	{

	}

	std::expected<std::vector<char>, bool> ReadFile(const std::string_view& path, ReadOptions options)
	{
		std::ifstream file(path.data(), std::ios::ate | std::ios::binary);
		if (!file.is_open())
			return std::unexpected(false);

		size_t fileSize = static_cast<size_t>(file.tellg());
		std::vector<char> buffer;

		size_t extraSize = options == ReadOptions::AddNullTerminator ? 1 : 0;
		buffer.reserve(fileSize + extraSize);

		buffer.resize(fileSize);
		
		file.seekg(0);
		file.read(buffer.data(), fileSize);

		if (options == ReadOptions::AddNullTerminator && buffer.back() != '\0')
			buffer.push_back('\0');

		file.close();
		return buffer;
	}
}