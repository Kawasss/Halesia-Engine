#include <fstream>

#include "io/IO.h"

namespace IO
{
	void WriteFile(const std::string& path)
	{

	}

	std::vector<char> ReadFile(const std::string& path, bool nullTerm)
	{
		std::ifstream file(path, std::ios::ate | std::ios::binary);
		if (!file.is_open())
			throw std::runtime_error("Failed to open the file at " + path);

		size_t fileSize = (size_t)file.tellg();
		std::vector<char> buffer(fileSize);

		file.seekg(0);
		file.read(buffer.data(), fileSize);
		if (nullTerm) buffer.push_back('\0');

		file.close();
		return buffer;
	}
}