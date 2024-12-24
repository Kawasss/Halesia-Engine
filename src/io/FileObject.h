#pragma once
#include <string>

#include "FileMesh.h"
#include "FileRigidBody.h"

using FileString = std::string;

struct FileObject
{
	FileString name;
	FileMesh mesh;
	FileRigidBody rigidBody;
};