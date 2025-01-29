#include <Windows.h>
#include <compressapi.h> // should switch to lz4

#include "core/UniquePointer.h"

#include "io/BinaryReader.h"
#include "io/FileFormat.h"

#pragma pack(push, 1)
struct CompressionNode
{
	NodeType type = NODE_TYPE_NONE;
	NodeSize nodeSize = 0;
	uint64_t fileSize = 0; // size of the rest of the file (uncompressed)
	uint32_t mode = 0;
};
#pragma pack(pop)

BinaryReader::BinaryReader(const std::string& source) : input(std::ifstream(source, std::ios::in | std::ios::binary)) {}

void BinaryReader::Read(char* ptr, size_t size)
{
	memcpy(ptr, stream.data() + pointer, size);
	pointer += size;
}

BinaryReader& BinaryReader::operator>>(std::string& str) // this expects the string in the file to be null terminated
{
	str = "";
	while (stream[pointer] != '\0')
		str += stream[pointer++];
	pointer++; // skip over the null
	return *this;
}

void BinaryReader::DecompressFile()
{
	CompressionNode compression{};

	input.read(reinterpret_cast<char*>(&compression), sizeof(compression));

	if (compression.type != NODE_TYPE_COMPRESSION)
		throw std::runtime_error("Cannot determine the needed compression information");

	size_t compressedSize = static_cast<size_t>(input.seekg(0, std::ios::end).tellg()) - sizeof(CompressionNode);

	stream.resize(compression.fileSize);
	UniquePointer<char> compressed = new char[compressedSize];

	ReadCompressedData(compressed.Get(), compressedSize);

	input.close();

	size_t uncompressedSize = DecompressData(compressed.Get(), stream.data(), compression.mode, compressedSize, compression.fileSize);

	if (compression.fileSize != uncompressedSize)
		throw std::runtime_error("Could not properly decompress the file: there is a mismatch between the reported sizes (" + std::to_string(compression.fileSize) + " != " + std::to_string(uncompressedSize) + ")");
}

void BinaryReader::ReadCompressedData(char* src, size_t size)
{
	input.seekg(sizeof(CompressionNode), std::ios::beg);
	input.read(src, size);
}

size_t BinaryReader::DecompressData(char* src, char* dst, uint32_t mode, size_t size, size_t expectedSize)
{
	size_t uncompressedSize = 0;
	DECOMPRESSOR_HANDLE decompressor;

	if (!CreateDecompressor(mode, NULL, &decompressor))
		throw std::runtime_error("Cannot create a compressor"); // XPRESS is fast but not the best compression, XPRESS with huffman has better compression but is slower, MSZIP uses more resources and LZMS is slow. its Using xpress right now since its the fastest
	if (!Decompress(decompressor, src, size, dst, expectedSize, &uncompressedSize))
		throw std::runtime_error("Cannot decompress");
	if (!CloseDecompressor(decompressor))
		throw std::runtime_error("Cannot close a compressor");

	return uncompressedSize;
}

void BinaryReader::Reset()
{
	pointer = 0;
}