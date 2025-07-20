#pragma once
#include <string_view>

using HANDLE = void*;

class ReadWriteFile
{
public:
	enum class Method
	{
		Begin   = 0,
		Current = 1,
		End     = 2,
	};

	enum class OpenMethod
	{
		Clear,
		Append,
	};

	ReadWriteFile(const std::string_view& file, OpenMethod method);
	~ReadWriteFile();

	bool IsValid() const;

	void Write(const char* src, unsigned long count) const;
	bool Read(char* dst, unsigned long count) const; // returns false if it has read nothing but the end of the file or an error has occured, otherwise true

	int64_t SeekG(int64_t index, ReadWriteFile::Method method) const;
	int64_t GetG() const;

	size_t GetFileSize() const;

private:
	union FileSize;

	HANDLE handle = reinterpret_cast<void*>(-1);
};