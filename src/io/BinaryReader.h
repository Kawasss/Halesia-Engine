#pragma once
#include <string>
#include <vector>
#include <fstream>

#include "FileBase.h"

class BinaryReader
{
public:
	BinaryReader() = default;
	BinaryReader(const std::string& source);

	void Read(char* ptr, size_t size);

	void DecompressFile();

	bool IsAtEndOfFile() { return pointer >= stream.size() - 1; }
	size_t GetPointer() const { return pointer; }

	BinaryReader& operator>>(std::string& str); // this expects the string in the file to be null terminated

	template<typename Type>
	BinaryReader& operator>>(Type& in)
	{
		if constexpr (std::is_base_of_v<FileBase, Type>) // this check has to be done here, declaring a seperate method for derived FileBase structs doesnt work
		{
			in.Read(*this);
		}
		else
		{
			char* dst = reinterpret_cast<char*>(&in);
			Read(dst, sizeof(Type));
		}
		return *this;
	}

	template<typename Type>
	BinaryReader& operator>>(std::vector<Type>& vec) // this expects the vector to already be resized to the correct size
	{
		char* dst = reinterpret_cast<char*>(vec.data());
		Read(dst, sizeof(Type) * vec.size());

		return *this;
	}

	void Reset();

private:
	void ReadCompressedData(char* src, size_t size);
	size_t DecompressData(char* src, char* dst, uint32_t mode, size_t size, size_t expectedSize);

	size_t pointer = 0;
	std::vector<char> stream;
	std::ifstream input;
};
