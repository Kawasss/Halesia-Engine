#pragma once
#include "FileArray.h"

class Texture;
struct Material;

using uint = unsigned int;

struct FileImage
{
	FileArray<char> data;

	static FileImage CreateFrom(Texture* tex);

	bool IsDefault() const 
	{ 
		return data.IsEmpty();
	}
};

struct FileMaterial
{
	bool isLight = false;

	FileImage albedo;
	FileImage normal;
	FileImage metallic;
	FileImage roughness;
	FileImage ambientOccl;

	static FileMaterial CreateFrom(const Material& mat);

	bool IsDefault() const
	{
		return !isLight && albedo.IsDefault() && normal.IsDefault() && metallic.IsDefault() && roughness.IsDefault() && ambientOccl.IsDefault();
	}
};