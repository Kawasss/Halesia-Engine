export module IO.ReadWriteFile;

import std;

export import IO.BasicStream;

using HANDLE = void*;

export class ReadWriteFile : public BasicStream
{
public:
	enum class OpenMethod
	{
		Clear,
		Append,
	};

	ReadWriteFile(const std::string_view& file, OpenMethod method);

	bool IsValid() const override;

	bool Write(const char* src, std::size_t count) override;
	bool Read(char* dst, std::size_t count) override; // returns false if it has read nothing but the end of the file or an error has occured, otherwise true

	void StartReading() override;
	void StopReading() override;

	void StartWriting() override;
	void StopWriting() override;

	std::int64_t SeekG(std::int64_t index, SeekMethod method) override;
	std::int64_t GetG() override;

	std::size_t GetSize() const override;

private:
	struct HandleDeleter
	{
		void operator()(void* ptr) const;
	};

	OpenMethod method;
	std::string file;
	std::unique_ptr<void, HandleDeleter> handle;
};