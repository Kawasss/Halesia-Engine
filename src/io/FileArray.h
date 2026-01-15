#pragma once
#include <vector>

template<typename T>
struct FileArray
{
	using ValueType = T;

	static FileArray CreateFrom(const std::vector<ValueType>& vec)
	{
		FileArray ret;
		ret.data = vec;
		return ret;
	}

	bool IsEmpty() const
	{
		return data.empty();
	}

	size_t GetSize() const
	{
		return data.size();
	}

	std::vector<ValueType> data;
};