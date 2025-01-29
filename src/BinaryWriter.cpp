#pragma comment(lib, "cabinet.lib") // needed for compression
#include <Windows.h>
#include <compressapi.h> // should switch to lz4

#include "core/UniquePointer.h"

#include "io/BinaryWriter.h"

BinaryWriter::BinaryWriter(std::string destination) : output(std::ofstream(destination, std::ios::binary)) {}

void BinaryWriter::Write(const char* ptr, size_t size)
{
	WriteToStream(ptr, size);
}

void BinaryWriter::WriteToStream(const char* src, size_t size)
{
	if (base + size > stream.size())
		stream.resize(base + size);

	memcpy(&stream[base], src, size);
	base += size;
}

void BinaryWriter::SetBase(size_t pos)
{
	base = pos;
}

size_t BinaryWriter::GetBase() const
{
	return base;
}

void BinaryWriter::WriteToFileCompressed()
{
	UniquePointer<char> compressed = new char[stream.size()]; // size is the max size, not the actual compressed size

	size_t compressedSize = 0;
	COMPRESSOR_HANDLE compressor;

	if (!CreateCompressor(COMPRESS_ALGORITHM_XPRESS, NULL, &compressor)) 
		throw std::runtime_error("Cannot create a compressor "); // XPRESS is fast but not the best compression, XPRESS with huffman has better compression but is slower, MSZIP uses more resources and LZMS is slow. its Using xpress right now since its the fastest
	
	if (!Compress(compressor, stream.data(), stream.size(), compressed.Get(), stream.size(), &compressedSize))
		throw std::runtime_error((std::string)"Cannot compress " + std::to_string(GetLastError()));
	
	if (!CloseCompressor(compressor)) 
		throw std::runtime_error("Cannot close a compressor"); // currently not checking the return value

	output.write(compressed.Get(), compressedSize);
	output.close();
}

#include <iostream>

void BinaryWriter::WriteFileBase(const FileBase& file)
{
	// FileBases are written based on nodes: the beginning of the node contains an identifier and the size of the node.
	// the size of the node is determined after the node has been written, so the steps of writing a FileBase looks like this:
	//
	// 1. calculate the location of the node header:
	//   - the identifer is at the base of the node
	//   - the size of the node is after the identifier, base + sizeof(uint16_t)
	// 2. write the node
	// 3. calculate the size of the node: currentLocation - (nodeBase + sizeof(NodeHeader))
	// 4. go back to the location of the nodes size and write the calculated size

	size_t sizePos = base + sizeof(uint16_t);
	file.Write(*this);
	size_t end = base;

	size_t size = end - (sizePos + sizeof(uint64_t));

	std::cout << "calculated node size " << size << '\n';

	SetBase(sizePos);
	WriteToStream(reinterpret_cast<char*>(&size), sizeof(size));
	SetBase(end);
}

void BinaryWriter::WriteToFileUncompressed()
{
	output.write(stream.data(), stream.size());
	output.close();
}

size_t BinaryWriter::GetCurrentSize() const
{
	return stream.size(); 
}