export module Renderer.RenderableMesh;

import "../glm.h";

import std;

import Renderer.StorageBuffer;
import Renderer.Vertex;
import Renderer.BLAS;

export enum RenderableMeshFlagBits
{
	RenderableMeshFlagNone = 0,
	RenderableMeshFlagNoCulling = 1 << 0,
	RenderableMeshFlagNoRayTracing = 1 << 1,
};
export using RenderableMeshFlags = std::underlying_type_t<RenderableMeshFlagBits>;

export struct RenderableMesh
{
	glm::mat4 transform;

	StorageBuffer<Vertex>::Memory dVertexMemory = 0;
	StorageBuffer<Vertex>::Memory vertexMemory  = 0;

	StorageBuffer<std::uint32_t>::Memory indexMemory = 0;

	std::shared_ptr<BottomLevelAccelerationStructure> BLAS;

	std::uint32_t materialIndex = 0;
	float uvScale = 1.0f;

	std::uint32_t faceCount   = 0;
	std::uint32_t vertexCount = 0;

	RenderableMeshFlags flags = RenderableMeshFlagNone;

	bool ShouldBeNotRayTraced() const
	{
		return flags & RenderableMeshFlagNoRayTracing || BLAS == nullptr;
	}

	bool ShouldNotCull() const
	{
		return flags & RenderableMeshFlagNoCulling;
	}
};