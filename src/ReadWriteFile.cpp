#include <filesystem>
#include <Windows.h>

#include "io/ReadWriteFile.h"

ReadWriteFile::ReadWriteFile(const std::string_view& file, OpenMethod method)
{
	handle = ::CreateFileA(file.data(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, nullptr, method == OpenMethod::Append ? OPEN_ALWAYS : CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
}

ReadWriteFile::~ReadWriteFile()
{
	if (handle != INVALID_HANDLE_VALUE)
		CloseHandle(handle);
}

bool ReadWriteFile::IsValid() const
{
	return handle != INVALID_HANDLE_VALUE;
}

bool ReadWriteFile::Read(char* dst, unsigned long count) const
{
	DWORD readCount = 0;
	BOOL res = ::ReadFile(handle, dst, count, &readCount, nullptr);

	return res && readCount != 0;
}

void ReadWriteFile::Write(const char* src, unsigned long count) const
{
	BOOL res = ::WriteFile(handle, src, count, nullptr, nullptr);
	if (res == FALSE);; // do whatever
}

int64_t ReadWriteFile::SeekG(int64_t index, ReadWriteFile::Method method) const
{
	LARGE_INTEGER g{}, ret{};
	g.QuadPart = index;

	::SetFilePointerEx(handle, g, &ret, static_cast<DWORD>(method));
	return ret.QuadPart;
}

int64_t ReadWriteFile::GetG() const
{
	return SeekG(0, Method::Current);
}

size_t ReadWriteFile::GetFileSize() const
{
	ULARGE_INTEGER size{};
	size.LowPart = ::GetFileSize(handle, &size.HighPart);
	return size.QuadPart;
}