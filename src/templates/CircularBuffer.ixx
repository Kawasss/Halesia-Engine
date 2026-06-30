export module Templates.CircularBuffer;

import std;

export template<typename T, std::size_t size>
struct CircularBuffer
{
public:
	CircularBuffer() : idx(0), buffer(size) {}

	void Add(const T& val)
	{
		if (buffer.size() < size)
			buffer.push_back(val);
		else
		{
			buffer[idx] = val;
			idx = (idx + 1) % size;
		}
	}

	std::vector<T>& GetInternalBuffer()
	{
		return buffer;
	}

	std::size_t GetSize() const
	{
		return buffer.size();
	}

	T& operator[](std::size_t i)
	{
		return buffer[i];
	}

private:
	std::vector<T> buffer;
	std::size_t idx;
};