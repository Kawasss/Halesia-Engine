#pragma once
#include <vector>

#include "FileBase.h"

using uint = unsigned int;

struct FileImage : FileBase
{
	uint width, height;
	std::vector<char> data;

	uint64 GetBinarySize() override;

	void Read(BinaryReader& reader) override;
	void Write(BinaryWriter& writer) override;

	bool IsDefault() const 
	{ 
		return width == 0 || height == 0 || data.empty();
	}
};

struct FileMaterial : FileBase
{
	bool isLight = false;

	FileImage albedo;
	FileImage normal;
	FileImage metallic;
	FileImage roughness;
	FileImage ambientOccl;

	uint64 GetBinarySize() override;

	void Read(BinaryReader& reader) override;
	void Write(BinaryWriter& writer) override;

	bool IsDefault() const
	{
		return !isLight && albedo.IsDefault() && normal.IsDefault() && metallic.IsDefault() && roughness.IsDefault() && ambientOccl.IsDefault();
	}
};