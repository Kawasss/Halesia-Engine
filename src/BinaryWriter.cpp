#pragma comment(lib, "cabinet.lib") // needed for compression
#include <Windows.h>
#include <compressapi.h> // should switch to lz4

#include "core/UniquePointer.h"

#include "io/BinaryWriter.h"

BinaryWriter::BinaryWriter(std::string destination) : output(std::ofstream(destination, std::ios::binary)) {}

void BinaryWriter::Write(const char* ptr, size_t size)
{
	stream.write(ptr, size);
}

void BinaryWriter::WriteToFileCompressed()
{
	size_t size = stream.seekg(0, std::ios::end).tellg();
	stream.seekg(0, std::ios::beg);

	UniquePointer data       = new char[size];
	UniquePointer compressed = new char[size]; // size is the max size, not the actual compressed size

	stream.read(data.Get(), size); // copying the stream into a usable buffer

	size_t compressedSize = 0;
	COMPRESSOR_HANDLE compressor;

	if (!CreateCompressor(COMPRESS_ALGORITHM_XPRESS, NULL, &compressor)) 
		throw std::runtime_error("Cannot create a compressor "); // XPRESS is fast but not the best compression, XPRESS with huffman has better compression but is slower, MSZIP uses more resources and LZMS is slow. its Using xpress right now since its the fastest
	
	if (!Compress(compressor, data.Get(), size, compressed.Get(), size, &compressedSize))
		throw std::runtime_error((std::string)"Cannot compress " + std::to_string(GetLastError()));
	
	if (!CloseCompressor(compressor)) 
		throw std::runtime_error("Cannot close a compressor"); // currently not checking the return value

	output.write(compressed.Get(), compressedSize);
	output.close();
}

void BinaryWriter::WriteToFileUncompressed()
{
	size_t size = stream.seekg(0, std::ios::end).tellg();
	stream.seekg(0, std::ios::beg);
	char* data = new char[size];
	stream.read(data, size); // copying the stream into a usable buffer

	output.write(data, size);
	output.close();
	delete[] data;
}

size_t BinaryWriter::GetCurrentSize() 
{
	size_t ret = stream.seekg(0, std::ios::end).tellg(); 
	stream.seekg(0); 
	return ret; 
}