#pragma once
#include <vector>

#include "FileBase.h"
#include "FileArray.h"

class Texture;
struct Material;

using uint = unsigned int;

struct FileImage : FileBase
{
	uint width = 0, height = 0;
	FileArray<char> data;

	static FileImage CreateFrom(Texture* tex);

	void Read(BinaryReader& reader) override;
	void Write(BinaryWriter& writer) const override;

	bool IsDefault() const 
	{ 
		return width == 0 || height == 0 || data.IsEmpty();
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

	void Read(BinaryReader& reader) override;
	void Write(BinaryWriter& writer) const override;

	bool IsDefault() const
	{
		return !isLight && albedo.IsDefault() && normal.IsDefault() && metallic.IsDefault() && roughness.IsDefault() && ambientOccl.IsDefault();
	}
};