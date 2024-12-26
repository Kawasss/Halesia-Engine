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

	uint64 GetBinarySize() const override
	{
		return data.size() * sizeof(ValueType);
	}

	void Write(BinaryWriter& writer) const override // file arrays dont write their node type
	{
		writer << GetBinarySize() << data.size() << data;
	}

	void Read(BinaryReader& reader) override
	{
		uint64 size;
		reader >> size; // read and discard the node size
		reader >> size;
		data.resize(size);
		reader >> data;
	}

	std::vector<ValueType> data;
};