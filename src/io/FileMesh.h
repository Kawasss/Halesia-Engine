#pragma once
#include <vector>

#include "FileBase.h"
#include "FileArray.h"

#include "../renderer/Vertex.h"

struct Mesh;

using uint = unsigned int;

struct FileMesh : FileBase
{
	static FileMesh CreateFrom(const Mesh& mesh);

	void Write(BinaryWriter& writer) const override;
	void Read(BinaryReader& reader) override;

	uint materialIndex;

	FileArray<Vertex> vertices;
	FileArray<uint32_t> indices;
};