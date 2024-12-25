#pragma once
#include <string>
#include <fstream>
#include <sstream>
#include <vector>

#include "FileBase.h"

class BinaryWriter
{
public:
	BinaryWriter(std::string destination);

	void Write(const char* ptr, size_t size);

	void WriteToFileCompressed();
	void WriteToFileUncompressed();

	size_t GetCurrentSize();

	template<typename Type>
	BinaryWriter& WriteDataToFile(const Type& data)
	{
		output.write(reinterpret_cast<const char*>(&data), sizeof(Type));
		return *this;
	}

	template<typename Type>
	BinaryWriter& operator<<(const Type& value)
	{
		if constexpr (std::is_base_of_v<FileBase, Type>)
		{
			value.Write(*this);
		}
		else
		{
			stream.write(reinterpret_cast<const char*>(&value), sizeof(Type));
		}
		return *this;
	}

	template<typename Type>
	BinaryWriter& operator<<(const std::vector<Type>& vector)
	{
		if (vector.empty()) return *this;
		stream.write((const char*)&vector[0], sizeof(Type) * vector.size());
		return *this;
	}

	BinaryWriter& operator<<(const std::string& str)
	{
		stream.write(str.c_str(), str.size() + 1); // also writes the null character
		return *this;
	}

private:
	std::stringstream stream;
	std::ofstream output;
};