#pragma once
#include <vector>

template<typename T> 
struct BufferView
{
public:
	using Iterator = T*;
	using Reference = T&;
	using ConstReference = const T&;

	BufferView() = default;
	BufferView(T* start, unsigned int size) : start(start), size(size) {}
	BufferView(const std::vector<T>& vec)   : start(vec.data()), size(vec.size()) {}

	Reference      operator[](size_t index)       { return start[index]; }
	ConstReference operator[](size_t index) const { return start[index]; }

	Iterator begin() const { return start; }
	Iterator end()   const { return start + size; }

private:
	Iterator start = nullptr;
	unsigned int size = 0;
};