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

bool ReadWriteFile::Read(char* dst, std::size_t count)
{
	DWORD readCount = 0;
	BOOL res = ::ReadFile(handle.get(), dst, static_cast<DWORD>(count), &readCount, nullptr);

	return res && readCount != 0;
}

bool ReadWriteFile::Write(const char* src, std::size_t count)
{
	return ::WriteFile(handle.get(), src, static_cast<DWORD>(count), nullptr, nullptr);
}

std::int64_t ReadWriteFile::SeekG(std::int64_t index, SeekMethod method)
{
	LARGE_INTEGER g{}, ret{};
	g.QuadPart = index;

	::SetFilePointerEx(handle.get(), g, &ret, static_cast<DWORD>(method));
	return ret.QuadPart;
}

std::int64_t ReadWriteFile::GetG()
{
	return SeekG(0, SeekMethod::Current);
}

std::size_t ReadWriteFile::GetSize() const
{
	ULARGE_INTEGER size{};
	size.LowPart = ::GetFileSize(handle.get(), &size.HighPart);
	return size.QuadPart;
}