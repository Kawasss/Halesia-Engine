export module Renderer.SimpleMesh;

import std;

import Renderer.StorageBuffer;
import Renderer.Vertex;

export class SimpleMesh // a mesh with only vertices, no indices, no material
{
public:
	SimpleMesh() = default;
	static SimpleMesh Create(const std::span<const Vertex>& vertices);

	SimpleMesh& operator=(SimpleMesh&& rhs) noexcept;
	SimpleMesh(const SimpleMesh&) = delete;

	StorageBuffer<Vertex>::Memory vertexMemory = 0;
	StorageBuffer<Vertex>::Memory defVertexMemory = 0;

	void Destroy();

private:
	SimpleMesh(const std::span<const Vertex>& vertices);
};