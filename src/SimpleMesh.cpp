module;

#include "renderer/Vertex.h"

module Renderer.SimpleMesh;

import std;

import Renderer.StorageBuffer;
import Renderer;

SimpleMesh SimpleMesh::Create(const std::span<const Vertex>& vertices)
{
	return SimpleMesh(vertices);
}

SimpleMesh& SimpleMesh::operator=(SimpleMesh&& rhs) noexcept
{
	std::swap(defVertexMemory, rhs.defVertexMemory);
	std::swap(vertexMemory, rhs.vertexMemory);

	rhs.Destroy(); // this call should not really be necessary, but just in case

	return *this;
}

SimpleMesh::SimpleMesh(const std::span<const Vertex>& vertices)
{
	defVertexMemory = Renderer::g_defaultVertexBuffer.SubmitNewData(vertices);
	vertexMemory    = Renderer::g_vertexBuffer.SubmitNewData(vertices);
}

void SimpleMesh::Destroy()
{
	if (defVertexMemory != 0)
		Renderer::g_defaultVertexBuffer.DestroyData(defVertexMemory);
	if (vertexMemory != 0)
		Renderer::g_vertexBuffer.DestroyData(vertexMemory);
}