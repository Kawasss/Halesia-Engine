#pragma once
#include <vector>

#include "FileBase.h"
#include "FileFormat.h"

#include "BinaryReader.h"
#include "BinaryWriter.h"

template<typename T>
struct FileArray : FileBase
{
	using ValueType = T;

	static FileArray CreateFrom(const std::vector<ValueType>& vec)
	{
		FileArray ret;
		ret.data = vec;
		return ret;
	}

	void Write(BinaryWriter& writer) const override // file arrays dont write their node type
	{
		writer << NODE_TYPE_ARRAY << 0ULL << data.size() << data;
	}

	void Read(BinaryReader& reader) override
	{
		size_t vSize;
		reader >> vSize; // read and discard the node size

		if (vSize == 0)
			return;

		data.resize(vSize);
		reader >> data;
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