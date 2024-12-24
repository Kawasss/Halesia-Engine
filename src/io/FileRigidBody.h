#pragma once
#include "FileShape.h"

using byte = unsigned char;

struct FileRigidBody
{
	enum class Type : byte
	{
		None = 0,
		Static = 1,
		Dynamic = 2,
		Kinematic = 3,
	};

	Type type = Type::None;
	FileShape shape;
};