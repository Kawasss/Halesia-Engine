#include <fstream>

#include "io/IO.h"

namespace IO
{
	void WriteFile(const std::string_view& path)
	{

	}

	std::vector<char> ReadFile(const std::string_view& path, bool nullTerm)
	{
		std::ifstream file(path.data(), std::ios::ate | std::ios::binary);
		if (!file.is_open())
			throw std::runtime_error("Failed to open the file");

		size_t fileSize = (size_t)file.tellg();
		std::vector<char> buffer(fileSize + (nullTerm ? 1 : 0));

		file.seekg(0);
		file.read(buffer.data(), fileSize);

		file.close();
		return buffer;
	}
}