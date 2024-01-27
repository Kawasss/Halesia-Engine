#pragma once
#include "Material.h"
#include "StorageBuffer.h"
#include "../Vertex.h"
#include <mutex>

class BottomLevelAccelerationStructure;
struct MeshCreationData;
struct MaterialCreationData;

struct Mesh
{
	static std::vector<Material> materials;

	void Create(const MeshCreationData& creationData);
	void Destroy();

	std::string name = "NO_NAME";

	uint32_t materialIndex = 0;
	StorageMemory vertexMemory;
	StorageMemory indexMemory;

	BottomLevelAccelerationStructure* BLAS;

	std::vector<Vertex> vertices;
	std::vector<uint16_t> indices;

	int faceCount;
	glm::vec3 min, max, center, extents;

	void ResetMaterial(); // should make it so that it also deletes the material if no other mesh references it
	void ProcessMaterial(const MaterialCreationData& creationData);
	void Recreate();
	bool HasFinishedLoading();
	void AwaitGeneration();

	/// <summary>
	/// Sets the material for this mesh, any old mesh will be overridden.
	/// </summary>
	/// <param name="material"></param>
	void SetMaterial(Material material);

private:
	static std::mutex materialMutex;
	bool finished = false;
};