#include <lz4/lz4.h>
#include <cassert>

#include "io/DataArchiveFile.h"

// the dictionary is serialized like this:
//
// entry count: unsigned 32 bit
// entry count amount of entries:
//   string, starts with a 32 bit value dictating the string length
//   data offset from the beginning of the file: unsigned 64 bit
//   compressed size of the data: unsigned 64 bit
//   uncompressed size of the data: unsigned 64 bit
// every data block starts with a 64 bit value showing the uncompressed size

DataArchiveFile::DataArchiveFile(const std::string& file, OpenMethod method) : stream(file, static_cast<ReadWriteFile::OpenMethod>(method))
{
	ReadDictionaryFromDisk();
}

bool DataArchiveFile::IsValid() const
{
	return stream.IsValid();
}

bool DataArchiveFile::HasEntry(const std::string& identifier) const
{
	return dictionary.contains(identifier);
}

void DataArchiveFile::AddData(const std::string& identifier, const std::span<char const>& data)
{
	Metadata metadata{};
	metadata.isOnDisk = false;
	metadata.offset = 0; // offset is calculated when the data is written to disk
	metadata.compressed = CompressMemory(data);
	metadata.size = metadata.compressed.size();
	metadata.uncompressedSize = data.size();

	dictionary[identifier] = metadata;
}

std::expected<std::vector<char>, bool> DataArchiveFile::ReadData(const std::string& identifier)
{
	if (!dictionary.contains(identifier))
		return std::unexpected(false);

	Metadata metadata = dictionary[identifier];

	if (!metadata.isOnDisk)
		return DecompressMemory(metadata.compressed, metadata.uncompressedSize);

	if (metadata.offset + metadata.size >= stream.GetFileSize() || metadata.size == 0)
		return std::unexpected(false);

	return ReadFromDisk(metadata.offset, metadata.size);
}

std::expected<std::vector<char>, bool>  DataArchiveFile::ReadFromDisk(uint64_t offset, uint64_t size) const
{
	AsyncReadSession session = stream.BeginAsyncRead();

	uint64_t uncompressedSize = 0;
	session.Read(reinterpret_cast<char*>(&uncompressedSize), static_cast<long long>(offset), sizeof(uncompressedSize));

	std::vector<char> read(size);
	session.Read(read.data(), static_cast<long long>(offset + sizeof(uncompressedSize)), static_cast<unsigned long>(size));

	return DecompressMemory(read, uncompressedSize);
}

std::expected<std::vector<char>, bool> DataArchiveFile::DecompressMemory(const std::span<char const>& compressed, uint64_t uncompressedSize)
{
	if (uncompressedSize == compressed.size())
		return std::vector<char>(compressed.begin(), compressed.end());

	int bounds = LZ4_compressBound(static_cast<int>(uncompressedSize));
	std::vector<char> uncompressed(bounds);
	int decompressedCount = ::LZ4_decompress_safe(compressed.data(), uncompressed.data(), static_cast<int>(compressed.size()), bounds);

	if (decompressedCount < 0)
		return std::unexpected(false); // cannot garantuee that the data inside the buffer is safe at this point

	return uncompressed;
}

std::vector<char> DataArchiveFile::CompressMemory(const std::span<char const>& uncompressed)
{
	std::vector<char> ret(uncompressed.size());
	int size = static_cast<int>(uncompressed.size());

	int compressedCount = LZ4_compress_default(uncompressed.data(), ret.data(), size, size);

	if (compressedCount == 0)
		return std::vector<char>(uncompressed.begin(), uncompressed.end());

	if (compressedCount != size)
		ret.resize(compressedCount);

	return ret;
}

void DataArchiveFile::ReadDictionaryFromDisk()
{
	stream.SeekG(0, ReadWriteFile::Method::Begin);

	uint32_t entryCount = 0;
	bool notEOF = stream.Read(reinterpret_cast<char*>(&entryCount), sizeof(entryCount));
	if (!notEOF)
		return;

	for (uint32_t i = 0; i < entryCount; i++)
	{
		uint32_t stringLength = 0;
		std::string identifier;
		Metadata metadata{};

		bool success = true;

		success = success && stream.Read(reinterpret_cast<char*>(&stringLength), sizeof(stringLength));

		identifier.resize(stringLength);
		success = success && stream.Read(identifier.data(), stringLength);
		success = success && stream.Read(reinterpret_cast<char*>(&metadata.offset), sizeof(metadata.offset));
		success = success && stream.Read(reinterpret_cast<char*>(&metadata.size), sizeof(metadata.size));

		if (!success || identifier.empty())
			return;

		dictionary[identifier] = metadata;
	}
}

void DataArchiveFile::ClearDictionary()
{
	dictionary.clear();
}

void DataArchiveFile::WriteToFile()
{
	WriteDictionaryToDisk();
	WriteDataEntriesToDisk();
}

void DataArchiveFile::WriteDictionaryToDisk()
{
	if (!stream.IsValid())
		return;

	uint64_t totalOffset = GetBinarySizeOfDictionary();

	stream.SeekG(0, ReadWriteFile::Method::Begin);

	uint32_t entryCount = static_cast<uint32_t>(dictionary.size());
	stream.Write(reinterpret_cast<char*>(&entryCount), sizeof(entryCount));

	for (auto& [identifier, metadata] : dictionary)
	{
		uint32_t stringLength = static_cast<uint32_t>(identifier.size());

		metadata.offset = totalOffset;

		stream.Write(reinterpret_cast<char*>(&stringLength), sizeof(stringLength));
		stream.Write(identifier.c_str(), static_cast<unsigned long>(identifier.size()));
		stream.Write(reinterpret_cast<char*>(&metadata.offset), sizeof(metadata.offset));
		stream.Write(reinterpret_cast<char*>(&metadata.size), sizeof(metadata.size));

		totalOffset += metadata.size + sizeof(uint64_t); // each entry starts with an extra 64 bits !!
	}
}

void DataArchiveFile::WriteDataEntriesToDisk()
{
	if (!stream.IsValid())
		return;

	for (const auto& [identifier, metadata] : dictionary)
	{
		if (metadata.isOnDisk)
			continue;

		assert(metadata.size == metadata.compressed.size());

		stream.SeekG(static_cast<int64_t>(metadata.offset), ReadWriteFile::Method::Begin);
		stream.Write(reinterpret_cast<const char*>(&metadata.uncompressedSize), sizeof(metadata.uncompressedSize));
		stream.Write(metadata.compressed.data(), static_cast<unsigned long>(metadata.size));
	}
}

static uint64_t BinarySizeOfString(const std::string_view& str)
{
	return str.size() * sizeof(char) + sizeof(uint32_t);
}

uint64_t DataArchiveFile::GetBinarySizeOfDictionary() const
{
	uint64_t size = sizeof(uint32_t);
	for (const auto& [identifier, metadata] : dictionary)
	{
		size += BinarySizeOfString(identifier);
		size += sizeof(metadata.offset);
		size += sizeof(metadata.size);
	}
	return size;
}

DataArchiveFile::Iterator DataArchiveFile::begin()
{
	return Iterator(dictionary.begin(), *this);
}

DataArchiveFile::Iterator DataArchiveFile::end()
{
	return Iterator(dictionary.end(), *this);
}

DataArchiveFile::Iterator::Iterator(const std::map<std::string, Metadata>::iterator& it, DataArchiveFile& parent) : internal(it), parent(parent)
{

}

DataArchiveFile::Iterator DataArchiveFile::Iterator::operator++()
{
	return Iterator(internal++, parent);
}

DataArchiveFile::DataEntry DataArchiveFile::Iterator::operator*() const
{
	std::expected<std::vector<char>, bool> read = parent.ReadData(internal->first);

	return read.has_value() ? DataEntry{ internal->first, *read } : DataEntry{ internal->first, {} };
}

bool DataArchiveFile::Iterator::operator!=(const Iterator& other) const
{
	return this->internal != other.internal;
}

bool DataArchiveFile::Iterator::operator==(const Iterator& other) const
{
	return this->internal == other.internal;
}