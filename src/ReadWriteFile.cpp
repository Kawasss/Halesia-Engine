#include <filesystem>
#include <Windows.h>

#include "io/ReadWriteFile.h"

AsyncReadSession::AsyncReadSession(const std::string_view& file)
{
	handle = ::CreateFileA(file.data(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	DWORD err = GetLastError();
}

AsyncReadSession::AsyncReadSession(AsyncReadSession&& other) noexcept
{
	std::swap(this->handle, other.handle);
}

AsyncReadSession::~AsyncReadSession()
{
	if (IsValid())
		CloseHandle(handle);
}

bool AsyncReadSession::Read(char* dst, int64_t offset, uint32_t count) const
{
	SeekG(offset);

	DWORD readCount = 0;
	BOOL res = ::ReadFile(handle, dst, count, &readCount, nullptr);

	return res && readCount;
}

bool AsyncReadSession::IsValid() const
{
	return handle != INVALID_HANDLE_VALUE;
}

void AsyncReadSession::SeekG(int64_t offset) const
{
	LARGE_INTEGER g{};
	g.QuadPart = offset;

	::SetFilePointerEx(handle, g, nullptr, FILE_BEGIN);
}

ReadWriteFile::ReadWriteFile(const std::string_view& file, OpenMethod method) : file(file)
{
	handle = ::CreateFileA(this->file.c_str(), GENERIC_READ/* | GENERIC_WRITE*/, FILE_SHARE_READ, nullptr, method == OpenMethod::Append ? OPEN_ALWAYS : CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
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

AsyncReadSession ReadWriteFile::BeginAsyncRead() const
{
	return AsyncReadSession(file);
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