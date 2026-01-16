export module IO.ReadWriteFile;

import std;

using HANDLE = void*;

export class ReadWriteFile
{
public:
	enum class Method
	{
		Begin = 0,
		Current = 1,
		End = 2,
	};

	enum class OpenMethod
	{
		Clear,
		Append,
	};

	ReadWriteFile(const std::string_view& file, OpenMethod method);

	bool IsValid() const;

	bool Write(const char* src, unsigned long count) const;
	bool Read(char* dst, unsigned long count) const; // returns false if it has read nothing but the end of the file or an error has occured, otherwise true

	void StartReading();
	void StopReading();

	void StartWriting();
	void StopWriting();

	std::int64_t SeekG(std::int64_t index, ReadWriteFile::Method method) const;
	std::int64_t GetG() const;

	std::size_t GetFileSize() const;

private:
	struct HandleDeleter
	{
		void operator()(void* ptr) const;
	};

	OpenMethod method;
	std::string file;
	std::unique_ptr<void, HandleDeleter> handle;
};

export class ReadSession
{
public:
	ReadSession(ReadWriteFile& file);
	~ReadSession();

private:
	ReadWriteFile& file;
};

export class WriteSession
{
public:
	WriteSession(ReadWriteFile& file);
	~WriteSession();

private:
	ReadWriteFile& file;
};