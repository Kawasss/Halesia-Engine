module;

#include <cassert>

module IO.BinaryStream;

import std;

BinaryStream::BinaryStream(const std::vector<char>& data) : data(data)
{

}

void BinaryStream::Read(char* dst, std::size_t count)
{
	assert(offset + count <= data.size());
	std::memcpy(dst, &data[offset], count);
	offset += count;
}

void BinaryStream::Write(const char* src, std::size_t count)
{
	data.insert(data.end(), src, src + count);
}

void BinaryStream::Clear()
{
	data.clear();
	offset = 0;
}

std::size_t BinaryStream::GetOffset() const
{
	return offset;
}

BinarySpan::BinarySpan(const BinaryStream& stream) : data(stream.data.begin(), stream.data.end())
{

}

BinarySpan::BinarySpan(const std::span<char const>& data) : data(data)
{
	
}

BinarySpan::BinarySpan(const std::vector<char>& data) : data(data)
{

}

void BinarySpan::Read(char* dst, size_t count) const
{
	assert(offset + count <= data.size());
	std::memcpy(dst, &data[offset], count);
	offset += count;
}

size_t BinarySpan::GetOffset() const
{
	return offset;
}