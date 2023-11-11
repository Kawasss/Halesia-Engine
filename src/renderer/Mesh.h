#pragma once
#include "Material.h"
#include "StorageBuffer.h"
#include "../Vertex.h"

class BottomLevelAccelerationStructure;
struct MeshCreationData;
struct MaterialCreationData;
struct VulkanCreationObject;

typedef VulkanCreationObject MeshCreationObject;
typedef VulkanCreationObject TextureCreationObject;

struct Mesh
{
	static std::vector<Material> materials;

	void Create(const MeshCreationObject& creationObject, const MeshCreationData& creationData);
	void Destroy();

	uint32_t materialIndex = 0;
	StorageMemory vertexMemory;
	StorageMemory indexMemory;

	BottomLevelAccelerationStructure* BLAS;

	std::vector<Vertex> vertices;
	std::vector<uint16_t> indices;

	int faceCount;
	glm::vec3 min, max, center, extents;

	void ProcessMaterial(const TextureCreationObject& creationObjects, const MaterialCreationData& creationData);
	void Recreate(const MeshCreationObject& creationObject);
	bool HasFinishedLoading();

	/// <summary>
	/// Sets the material for this mesh, any old mesh will be overridden.
	/// </summary>
	/// <param name="material"></param>
	void SetMaterial(Material material);
};