module;

#include <Windows.h>

module IO.ReadWriteFile;

import std;

void ReadWriteFile::HandleDeleter::operator()(void* ptr) const
{
	if (ptr != INVALID_HANDLE_VALUE)
		::CloseHandle(ptr);
}

ReadWriteFile::ReadWriteFile(const std::string_view& file, OpenMethod method) : file(file), method(method)
{

}

bool ReadWriteFile::IsValid() const
{
	return handle.get() != INVALID_HANDLE_VALUE;
}

void ReadWriteFile::StartReading()
{
	handle.reset(::CreateFileA(this->file.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, method == OpenMethod::Append ? OPEN_ALWAYS : CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
}

void ReadWriteFile::StopReading()
{
	handle.reset();
}

void ReadWriteFile::StartWriting()
{
	handle.reset(::CreateFileA(this->file.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, method == OpenMethod::Append ? OPEN_ALWAYS : CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
}

void ReadWriteFile::StopWriting()
{
	handle.reset();
}

bool ReadWriteFile::Read(char* dst, unsigned long count) const
{
	DWORD readCount = 0;
	BOOL res = ::ReadFile(handle.get(), dst, count, &readCount, nullptr);

	return res && readCount != 0;
}

bool ReadWriteFile::Write(const char* src, unsigned long count) const
{
	return ::WriteFile(handle.get(), src, count, nullptr, nullptr);
}

int64_t ReadWriteFile::SeekG(int64_t index, ReadWriteFile::Method method) const
{
	LARGE_INTEGER g{}, ret{};
	g.QuadPart = index;

	::SetFilePointerEx(handle.get(), g, &ret, static_cast<DWORD>(method));
	return ret.QuadPart;
}

int64_t ReadWriteFile::GetG() const
{
	return SeekG(0, Method::Current);
}

size_t ReadWriteFile::GetFileSize() const
{
	ULARGE_INTEGER size{};
	size.LowPart = ::GetFileSize(handle.get(), &size.HighPart);
	return size.QuadPart;
}

ReadSession::ReadSession(ReadWriteFile& file) : file(file)
{
	file.StartReading();
}

ReadSession::~ReadSession()
{
	file.StopReading();
}

WriteSession::WriteSession(ReadWriteFile& file) : file(file)
{
	file.StartWriting();
}

WriteSession::~WriteSession()
{
	file.StopWriting();
}