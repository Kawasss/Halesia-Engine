#pragma once
#include <vector>

#include "FileBase.h"

#include "../renderer/Vertex.h"

using uint = unsigned int;

struct FileMesh : FileBase
{
	uint materialIndex;

	std::vector<Vertex> vertices;
	std::vector<uint16_t> indices;
};