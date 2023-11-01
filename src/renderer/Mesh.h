#pragma once
#include "../SceneLoader.h"
#include "../CreationObjects.h"
#include "../Material.h"
#include "../ResourceManager.h"
#include "AccelerationStructures.h"

struct Mesh
{
	static std::vector<Material> materials;

	Mesh(const MeshCreationObject& creationObject, const MeshCreationData& creationData);
	void Destroy();

	uint32_t materialIndex;
	StorageMemory vertexMemory;
	StorageMemory indexMemory;

	BottomLevelAccelerationStructure* BLAS;

	std::vector<Vertex> vertices;
	std::vector<uint16_t> indices;

	int faceCount;
	glm::vec3 min, max, center, extents;

	void ProcessMaterial(const TextureCreationObject& creationObjects, const MaterialCreationData& creationData);
	void Recreate(const MeshCreationObject& creationObject);
};