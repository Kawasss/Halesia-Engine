export module IO.ReadWriteDevice;

import std;

import IO.BinaryStream;
import IO.ReadWriteFile;

/// <summary>
/// A read-write device is a device that can either read from a file or use the system memory
/// reading is disabled in memory mode
/// </summary>
export class ReadWriteDevice
{
public:
	/// <summary>
	/// 
	/// </summary>
	/// <param name="file">if empty, it will initialize in memory mode, otherwise in file mode</param>
	/// <param name="method"></param>
	ReadWriteDevice(const std::string_view& file, ReadWriteFile::OpenMethod method);

	bool Write(const char* src, unsigned long count);
	bool Read(char* dst, unsigned long count) const; // returns false if it has read nothing but the end of the file or an error has occured, otherwise true

	bool IsInFileMode() const;
	bool IsInMemoryMode() const;
	
	std::size_t GetSize() const;

	bool IsValid() const;

	std::int64_t SeekG(std::int64_t index, ReadWriteFile::Method method) const;

	std::optional<WriteSession> CreateWriteSession();
	std::optional<ReadSession> CreateReadSession();

private:
	std::variant<BinaryStream, ReadWriteFile> variant;
};