#pragma once
#include <string_view>

using HANDLE = void*;

class AsyncReadSession
{
public:
	AsyncReadSession(const std::string_view& file); // the file should already exist, since this doesnt check for that
	AsyncReadSession(AsyncReadSession&& other) noexcept;
	~AsyncReadSession();

	AsyncReadSession& operator=(AsyncReadSession&&) = delete;
	AsyncReadSession(const AsyncReadSession&) = delete;

	bool IsValid() const;

	bool Read(char* dst, int64_t offset, uint32_t count) const;

private:
	void SeekG(int64_t offset) const;

	HANDLE handle = reinterpret_cast<void*>(-1);
};

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

	AsyncReadSession BeginAsyncRead() const;

	int64_t SeekG(int64_t index, ReadWriteFile::Method method) const;
	int64_t GetG() const;

	size_t GetFileSize() const;

private:
	std::string file;
	HANDLE handle = reinterpret_cast<void*>(-1);
};