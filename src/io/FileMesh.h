#pragma once
#include <hsl/Reference.h>

using uint = unsigned int;

struct SharedBuffer
{
	uint offset, size;
	hsl::Reference<char> buffer;
};

struct FileMesh
{
	int materialIndex;

	SharedBuffer vertices;
	SharedBuffer indices;
};