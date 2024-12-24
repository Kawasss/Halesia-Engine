#pragma once
#include <vector>

using uint = unsigned int;

struct FileImage
{
	uint width, height;
	std::vector<char> data;

	bool IsDefault() const 
	{ 
		return width == 0 || height == 0 || data.empty();
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
	FileImage height;

	bool IsDefault() const
	{
		return !isLight && albedo.IsDefault() && normal.IsDefault() && metallic.IsDefault() && roughness.IsDefault() && ambientOccl.IsDefault() && height.IsDefault();
	}
};