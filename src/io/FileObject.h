#pragma once
#include <string>

#include "FileMesh.h"
#include "FileRigidBody.h"

using FileString = std::string;

struct FileObject
{
	enum class InheritType
	{
		Base = 0,
		Mesh = 1,
	};

	FileString name;
	FileMesh mesh;
	FileRigidBody rigidBody;

	InheritType type;
};