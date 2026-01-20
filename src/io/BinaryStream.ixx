export module IO.BinaryStream;

import std;

#define DECLARE_WRITE_OPERATOR(type) BinaryStream& operator<<(const type& val)
#define DECLARE_READ_OPERATOR(type)  BinaryStream& operator>>(type& val)

export class BinaryStream
{
public:
	BinaryStream() = default;
	BinaryStream(const std::vector<char>& data); // copies the vector, does not assume ownership

	DECLARE_WRITE_OPERATOR(std::uint64_t);
	DECLARE_WRITE_OPERATOR(std::uint32_t);
	DECLARE_WRITE_OPERATOR(std::uint16_t);
	DECLARE_WRITE_OPERATOR(std::uint8_t);

	DECLARE_WRITE_OPERATOR(std::int64_t);
	DECLARE_WRITE_OPERATOR(std::int32_t);
	DECLARE_WRITE_OPERATOR(std::int16_t);
	DECLARE_WRITE_OPERATOR(std::int8_t);

	DECLARE_WRITE_OPERATOR(float);
	DECLARE_WRITE_OPERATOR(bool);
	DECLARE_WRITE_OPERATOR(char);

	DECLARE_READ_OPERATOR(std::uint64_t);
	DECLARE_READ_OPERATOR(std::uint32_t);
	DECLARE_READ_OPERATOR(std::uint16_t);
	DECLARE_READ_OPERATOR(std::uint8_t);

	DECLARE_READ_OPERATOR(std::int64_t);
	DECLARE_READ_OPERATOR(std::int32_t);
	DECLARE_READ_OPERATOR(std::int16_t);
	DECLARE_READ_OPERATOR(std::int8_t);

	DECLARE_READ_OPERATOR(float);
	DECLARE_READ_OPERATOR(bool);
	DECLARE_READ_OPERATOR(char);

	void Read(char* dst, size_t count);
	void Write(const char* src, size_t count); // appends
	void Clear();

	std::vector<char> data;

	std::size_t GetOffset() const; // only returns a usable number when reading

private:
	std::size_t offset = 0; // only used for reading
};

#undef DECLARE_READ_OPERATOR
#undef DECLARE_WRITE_OPERATOR

#define DECLARE_READ_OPERATOR(type)  const BinarySpan& operator>>(type& val) const

export class BinarySpan
{
public:
	BinarySpan(const BinaryStream& stream);
	BinarySpan(const std::span<char const>& data);
	BinarySpan(const std::vector<char>& data);

	DECLARE_READ_OPERATOR(std::uint64_t);
	DECLARE_READ_OPERATOR(std::uint32_t);
	DECLARE_READ_OPERATOR(std::uint16_t);
	DECLARE_READ_OPERATOR(std::uint8_t);

	DECLARE_READ_OPERATOR(std::int64_t);
	DECLARE_READ_OPERATOR(std::int32_t);
	DECLARE_READ_OPERATOR(std::int16_t);
	DECLARE_READ_OPERATOR(std::int8_t);

	DECLARE_READ_OPERATOR(float);
	DECLARE_READ_OPERATOR(bool);
	DECLARE_READ_OPERATOR(char);

	void Read(char* dst, std::size_t count) const;

	std::span<char const> data;

	size_t GetOffset() const;

private:
	mutable size_t offset = 0; // i think its fine to make this mutable since this variable is only ever used by the class itself and actual important part (the data) is never touched anyway
};

#undef DECLARE_READ_OPERATOR