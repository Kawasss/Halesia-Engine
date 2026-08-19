module;

#include <cassert>

module IO.BinaryStream;

import std;

BinaryStream::BinaryStream(const std::vector<char>& data) : data(data)
{

}

bool BinaryStream::Read(char* dst, std::size_t count)
{
	assert(offset + count <= data.size());
	std::memcpy(dst, &data[offset], count);
	offset += static_cast<std::int64_t>(count);

	return true;
}

bool BinaryStream::Write(const char* src, std::size_t count)
{
	data.insert(data.end(), src, src + count);
	offset += static_cast<std::int64_t>(count);
	return true;
}

void BinaryStream::Clear()
{
	data.clear();
	offset = 0;
}

std::int64_t BinaryStream::GetG()
{
	return static_cast<std::int64_t>(offset);
}

std::int64_t BinaryStream::SeekG(std::int64_t index, SeekMethod method)
{
	switch (method)
	{
	case SeekMethod::Begin:
		if (index >= 0)
			offset = index;
		break;
	case SeekMethod::Current:
	{
		std::int64_t res = offset + index;
		if (res >= 0)
			offset = res;
		break;
	}
	case SeekMethod::End:
	{
		std::int64_t end = std::max(static_cast<std::int64_t>(data.size()), offset);
		std::int64_t res = end + index;
		if (res >= 0)
			offset = res;
		break;
	}
	}
	return offset;
}

std::size_t BinaryStream::GetSize() const
{
	return data.size();
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