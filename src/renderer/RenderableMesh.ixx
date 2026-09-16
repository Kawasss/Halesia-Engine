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
	enum class PreferredLevelOfDetail
	{
		Active,
		Highest,
		Lowest,
	};

	struct Memory
	{
		StorageBuffer<Vertex>::Memory dVertexMemory = 0;
		StorageBuffer<Vertex>::Memory vertexMemory = 0;

		StorageBuffer<std::uint32_t>::Memory indexMemory = 0;

		std::shared_ptr<BottomLevelAccelerationStructure> BLAS;

		std::uint32_t faceCount = 0;
		std::uint32_t vertexCount = 0;
	};

	RenderableMesh() : priorityLod(activeLod) {}

	Memory activeLod;
	Memory lowestLod;
	Memory highestLod;

	const Memory& priorityLod;

	glm::mat4 transform;
	glm::mat4 prevTransform;

	std::uint32_t materialIndex = 0;
	float uvScale = 1.0f;

	RenderableMeshFlags flags = RenderableMeshFlagNone;

	const Memory& operator[](PreferredLevelOfDetail plod) const
	{
		switch (plod)
		{
		case PreferredLevelOfDetail::Active:  return activeLod;
		case PreferredLevelOfDetail::Highest: return highestLod;
		case PreferredLevelOfDetail::Lowest:  return lowestLod;
		}
		return activeLod;
	}

	bool ShouldBeNotRayTraced() const
	{
		return flags & RenderableMeshFlagNoRayTracing || activeLod.BLAS == nullptr;
	}

	bool ShouldNotCull() const
	{
		return flags & RenderableMeshFlagNoCulling;
	}
};