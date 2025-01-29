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

	void SetBase(size_t pos);

	size_t GetCurrentSize() const;
	size_t GetBase() const;

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
			WriteFileBase(value);
		}
		else
		{
			WriteToStream(reinterpret_cast<const char*>(&value), sizeof(Type));
		}
		return *this;
	}

	template<typename Type>
	BinaryWriter& operator<<(const std::vector<Type>& vector)
	{
		if (!vector.empty())
		{
			const char* src = reinterpret_cast<const char*>(&vector[0]);
			WriteToStream(src, sizeof(Type) * vector.size());
		}
		return *this;
	}

	template<>
	BinaryWriter& operator<<<std::string>(const std::string& str)
	{
		WriteToStream(str.c_str(), str.size() + 1); // also writes the null character
		return *this;
	}

private:
	void WriteFileBase(const FileBase& base);
	void WriteToStream(const char* src, size_t size);

	std::vector<char> stream;
	std::ofstream output;
	size_t base = 0;
};