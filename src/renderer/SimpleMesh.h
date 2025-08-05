#pragma once
#include <span>

#include "Vertex.h"

using StorageMemory = unsigned long long;

class SimpleMesh // a mesh with only vertices, no indices, no material
{
public:
	SimpleMesh() = default;
	static SimpleMesh Create(const std::span<const Vertex>& vertices);

	SimpleMesh& operator=(SimpleMesh&& rhs) noexcept;
	SimpleMesh(const SimpleMesh&) = delete;

	StorageMemory vertexMemory = 0;
	StorageMemory defVertexMemory = 0;

	void Destroy();

private:
	SimpleMesh(const std::span<const Vertex>& vertices);
};