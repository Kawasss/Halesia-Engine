#include <cassert>
#include <algorithm>
#include <type_traits>

#include "io/BinaryStream.h"

template<typename T>
concept PrimitiveOnly = std::is_fundamental_v<T>;

template<PrimitiveOnly T>
void WritePrimitiveToVector(std::vector<char>& vec, const T& val)
{
	constexpr size_t writeCount = sizeof(T) / sizeof(std::vector<char>::value_type);
	const char* pValue = reinterpret_cast<const char*>(&val);

	vec.insert(vec.end(), pValue, pValue + writeCount);
}

template<PrimitiveOnly T>
void ReadPrimitiveFromSpan(const std::span<char const>& vec, size_t& offset, T& val)
{
	constexpr size_t readCount = sizeof(T) / sizeof(std::vector<char>::value_type);
	const T* pValue = reinterpret_cast<const T*>(&vec[0] + offset);
	val = *pValue;

	offset += readCount;
}

BinaryStream::BinaryStream(const std::vector<char>& data) : data(data)
{

}

#define DEFINE_WRITE_OPERATOR(type)                     \
BinaryStream& BinaryStream::operator<<(const type& val) \
{                                                       \
	WritePrimitiveToVector(data, val);                  \
	return *this;                                       \
}                                                       \

#define DEFINE_READ_OPERATOR(type)                \
BinaryStream& BinaryStream::operator>>(type& val) \
{                                                 \
	ReadPrimitiveFromSpan(data, offset, val);   \
	return *this;                                 \
}                                                 \

DEFINE_WRITE_OPERATOR(uint64_t);
DEFINE_WRITE_OPERATOR(uint32_t);
DEFINE_WRITE_OPERATOR(uint16_t);
DEFINE_WRITE_OPERATOR(uint8_t);

DEFINE_WRITE_OPERATOR(int64_t);
DEFINE_WRITE_OPERATOR(int32_t);
DEFINE_WRITE_OPERATOR(int16_t);
DEFINE_WRITE_OPERATOR(int8_t);

DEFINE_WRITE_OPERATOR(float);
DEFINE_WRITE_OPERATOR(bool);
DEFINE_WRITE_OPERATOR(char);

DEFINE_READ_OPERATOR(uint64_t);
DEFINE_READ_OPERATOR(uint32_t);
DEFINE_READ_OPERATOR(uint16_t);
DEFINE_READ_OPERATOR(uint8_t);

DEFINE_READ_OPERATOR(int64_t);
DEFINE_READ_OPERATOR(int32_t);
DEFINE_READ_OPERATOR(int16_t);
DEFINE_READ_OPERATOR(int8_t);

DEFINE_READ_OPERATOR(float);
DEFINE_READ_OPERATOR(bool);
DEFINE_READ_OPERATOR(char);

#undef DEFINE_READ_OPERATOR
#undef DEFINE_WRITE_OPERATOR

void BinaryStream::Read(char* dst, size_t count)
{
	assert(offset + count <= data.size());
	memcpy(dst, &data[offset], count);
	offset += count;
}

void BinaryStream::Write(const char* src, size_t count)
{
	data.insert(data.end(), src, src + count);
}

void BinaryStream::Clear()
{
	data.clear();
	offset = 0;
}

size_t BinaryStream::GetOffset() const
{
	return offset;
}

BinarySpan::BinarySpan(const BinaryStream& stream) : data(stream.data)
{

}

BinarySpan::BinarySpan(const std::span<char const>& data) : data(data)
{
	
}

BinarySpan::BinarySpan(const std::vector<char>& data) : data(data)
{

}

#define DEFINE_READ_OPERATOR(type)                  \
const BinarySpan& BinarySpan::operator>>(type& val) const \
{                                                   \
	ReadPrimitiveFromSpan(data, offset, val);       \
	return *this;                                   \
}        

DEFINE_READ_OPERATOR(uint64_t);
DEFINE_READ_OPERATOR(uint32_t);
DEFINE_READ_OPERATOR(uint16_t);
DEFINE_READ_OPERATOR(uint8_t);

DEFINE_READ_OPERATOR(int64_t);
DEFINE_READ_OPERATOR(int32_t);
DEFINE_READ_OPERATOR(int16_t);
DEFINE_READ_OPERATOR(int8_t);

DEFINE_READ_OPERATOR(float);
DEFINE_READ_OPERATOR(bool);
DEFINE_READ_OPERATOR(char);

#undef DEFINE_READ_OPERATOR

void BinarySpan::Read(char* dst, size_t count) const
{
	assert(offset + count <= data.size());
	memcpy(dst, &data[offset], count);
	offset += count;
}

size_t BinarySpan::GetOffset() const
{
	return offset;
}