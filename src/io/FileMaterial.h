#pragma once
#include <vector>

#include "FileBase.h"

class Texture;
struct Material;

using uint = unsigned int;

struct FileImage : FileBase
{
	uint width, height;
	std::vector<char> data;

	static FileImage CreateFrom(Texture* tex);

	uint64 GetBinarySize() const override;

	void Read(BinaryReader& reader) override;
	void Write(BinaryWriter& writer) const override;

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

	static FileMaterial CreateFrom(const Material& mat);

	uint64 GetBinarySize() const override;

	void Read(BinaryReader& reader) override;
	void Write(BinaryWriter& writer) const override;

	bool IsDefault() const
	{
		return !isLight && albedo.IsDefault() && normal.IsDefault() && metallic.IsDefault() && roughness.IsDefault() && ambientOccl.IsDefault();
	}
};