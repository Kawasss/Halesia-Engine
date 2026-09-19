export module IO.DataArchiveFile;

import std;

import IO.ReadWriteFile;
import IO.BasicStream;

export class DataArchiveFile
{
public:
	enum EntryFlags
	{
		None = 0,
		Compression = 1 << 0,
		Checksum = 1 << 1,
	};

private:
	struct Metadata
	{
		bool isOnDisk = true;

		EntryFlags flags = EntryFlags::None;
		std::uint64_t offset = 0;
		std::uint64_t size = 0;
		std::uint64_t uncompressedSize = 0; // this is only used for data that isnt written to the disk yet
		std::uint32_t checksum = 0;
		std::vector<char> compressed;
	};

public:
	struct DataEntry
	{
		std::string_view identifier; // valid string as long as the DataArchiveFile is alive
		std::vector<char> data;      // data will be empty if the read has failed
	};

	class Iterator
	{
	public:
		Iterator(const std::map<std::string, Metadata>::iterator& it, DataArchiveFile& parent);

		Iterator operator++();

		bool operator!=(const Iterator& other) const;
		bool operator==(const Iterator& other) const;

		DataEntry operator*() const;

	private:
		std::map<std::string, Metadata>::iterator internal;

		DataArchiveFile& parent;
	};

	enum class OpenMethod
	{
		Clear = ReadWriteFile::OpenMethod::Clear,  //!< clear the file upon opening
		Append = ReadWriteFile::OpenMethod::Append, //!< do not clear the file
	};

	enum Result
	{
		Success,
		IdentifierNotFound,
		DecompressionFailed,
		InvalidReference, //!< the data pointing to the data associated with the identifier is not valid (i.e. out of bounds)
	};

	static DataArchiveFile LoadFromFile(const std::string_view& file, OpenMethod method);
	static DataArchiveFile CreateInMemory();

	/// <summary>
	/// Adds data to the archive. This will override any data that the identifier could already be holding
	/// </summary>
	/// <param name="identifier">the identifier to associate the data with</param>
	/// <param name="data">the data that should be bound to the identifier</param>
	void AddData(const std::string& identifier, const std::span<char const>& data);

	/// <summary>
	/// reads the data of associated with the given identifier
	/// </summary>
	/// <param name="identifier"></param>
	/// <returns>if no error took place the data, otherwise an error code</returns>
	std::expected<std::vector<char>, Result> ReadData(const std::string& identifier);

	bool IsValid() const;
	bool HasEntry(const std::string& identifier) const;

	void WriteToFile(); // writes the data that is only in RAM to disk, ignores any table entry thats already in the file
	void ClearDictionary();

	Iterator begin();
	Iterator end();

private:
	DataArchiveFile(std::unique_ptr<BasicStream>&& file);

	// the presence of the identifier is confirmed at this point, offset should be the offset from the start of the file
	std::expected<std::vector<char>, Result> ReadFromDisk(std::uint64_t offset, std::uint64_t size);
	std::uint64_t GetBinarySizeOfDictionary() const;

	// these two functions should always be called together, as 'WriteDictionaryToDisk()' calculates parameters that 'WriteDataEntriesToDisk()' requires
	void WriteDictionaryToDisk();
	void WriteDataEntriesToDisk();

	void ReadDictionaryFromDisk();
	void ReadEntryFromDisk();

	static std::expected<std::vector<char>, Result> DecompressMemory(const std::span<char const>& compressed, std::uint64_t uncompressedSize);
	static std::vector<char> CompressMemory(const std::span<char const>& uncompressed);

	std::map<std::string, Metadata> dictionary;
	std::unique_ptr<BasicStream> stream;
};