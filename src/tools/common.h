#pragma once
#include <vector>
#include <string>
#include <iostream>
#include <fstream>

inline std::vector<char> ReadFile(const std::string& filePath)
{
	std::ifstream file(filePath, std::ios::ate | std::ios::binary);
	if (!file.is_open())
		throw std::runtime_error("Failed to open the file at " + filePath);

	size_t fileSize = (size_t)file.tellg();
	std::vector<char> buffer(fileSize);

	file.seekg(0);
	file.read(buffer.data(), fileSize);

	file.close();
	return buffer;
}

inline std::vector<std::string> SeperateStringByChar(char seperator)
{
	std::vector<std::string> ret;
	return ret;
}