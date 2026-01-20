export module IO.BinaryStream;

import std;

template<typename T>
concept PrimitiveOnly = std::is_fundamental_v<T>;

export class BinaryStream
{
public:
	BinaryStream() = default;
	BinaryStream(const std::vector<char>& data); // copies the vector, does not assume ownership

	template<PrimitiveOnly T>
	BinaryStream& operator<<(const T& val)
	{
		constexpr size_t writeCount = sizeof(T) / sizeof(std::vector<char>::value_type);
		const char* pValue = reinterpret_cast<const char*>(&val);

		Write(pValue, writeCount);
		return *this;
	}

	template<PrimitiveOnly T>
	BinaryStream& operator>>(T& val)
	{
		constexpr size_t readCount = sizeof(T) / sizeof(std::vector<char>::value_type);
		const T* pValue = reinterpret_cast<const T*>(&data[0] + offset);
		val = *pValue;

		offset += readCount;
		return *this;
	}

	void Read(char* dst, size_t count);
	void Write(const char* src, size_t count); // appends
	void Clear();

	std::vector<char> data;

	std::size_t GetOffset() const; // only returns a usable number when reading

private:
	std::size_t offset = 0; // only used for reading
};

export class BinarySpan
{
public:
	BinarySpan(const BinaryStream& stream);
	BinarySpan(const std::span<char const>& data);
	BinarySpan(const std::vector<char>& data);

	template<PrimitiveOnly T>
	const BinarySpan& operator>>(T& val) const
	{
		constexpr size_t readCount = sizeof(T) / sizeof(std::vector<char>::value_type);
		const T* pValue = reinterpret_cast<const T*>(&data[0] + offset);
		val = *pValue;

		offset += readCount;
		return *this;
	}

	void Read(char* dst, std::size_t count) const;

	std::span<const char> data;

	size_t GetOffset() const;

private:
	mutable size_t offset = 0; // i think its fine to make this mutable since this variable is only ever used by the class itself and actual important part (the data) is never touched anyway
};