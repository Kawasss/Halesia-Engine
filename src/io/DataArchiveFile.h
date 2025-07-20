#pragma once
#include <string>
#include <expected>
#include <span>
#include <vector>
#include <map>

#include "ReadWriteFile.h"

class DataArchiveFile
{
private:
	struct Metadata
	{
		bool isOnDisk = true;

		uint64_t offset = 0;
		uint64_t size = 0;
		uint64_t uncompressedSize = 0; // this is only used for data that isnt written to the disk yet
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
		Clear  = ReadWriteFile::OpenMethod::Clear, // clear the file upon opening
		Append = ReadWriteFile::OpenMethod::Append, // do not clear the file
	};

	DataArchiveFile(const std::string& file, OpenMethod method);

	// will override any previous data assigned to the identifier
	void AddData(const std::string& identifier, const std::span<char const>& data);
	std::expected<std::vector<char>, bool> ReadData(const std::string& identifier);

	bool IsValid() const;

	void WriteToFile(); // writes the data that is only in RAM to disk, ignores any table entry thats already in the file
	void ClearDictionary();

	Iterator begin();
	Iterator end();

private:
	// the presence of identifier is confirmed at this point, offset should be the offset from the start of the file
	std::expected<std::vector<char>, bool>  ReadFromDisk(uint64_t offset, uint64_t size) const;
	uint64_t GetBinarySizeOfDictionary() const;

	// these two functions should always be called together, as 'WriteDictionaryToDisk()' calculates parameters that 'WriteDataEntriesToDisk()' requires
	void WriteDictionaryToDisk();
	void WriteDataEntriesToDisk();

	void ReadDictionaryFromDisk();

	static std::expected<std::vector<char>, bool> DecompressMemory(const std::span<char const>& compressed, uint64_t uncompressedSize);
	static std::vector<char> CompressMemory(const std::span<char const>& uncompressed);

	std::map<std::string, Metadata> dictionary;
	ReadWriteFile stream;
};