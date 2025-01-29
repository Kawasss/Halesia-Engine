#pragma once
#include "FileBase.h"

using byte = unsigned char;

struct FileShape : FileBase
{
	enum class Type : byte
	{
		None    = 0,
		Sphere  = 1,
		Box     = 2,
		Capsule = 3,
		Plane   = 4,
	};

	Type type = Type::None;
	float x, y, z;

	void Write(BinaryWriter& writer) const override;
	void Read(BinaryReader& reader) override;
};