#pragma once
#include <memory>
#include <vector>
#include <mutex>

#include "Material.h"
#include "Vertex.h"
#include "AccelerationStructures.h"

#include "../io/FwdDclCreationData.h"

class BottomLevelAccelerationStructure;
struct MeshCreationData;

using StorageMemory = unsigned long long;

struct Mesh
{
	static Handle AddMaterial(const Material& material); // returns the handle to the material

	static std::vector<Material> materials;

	void Create(const MeshCreationData& creationData);
	void Destroy();

	void CopyFrom(const Mesh& mesh);

	StorageMemory vertexMemory;
	StorageMemory indexMemory;
	StorageMemory defaultVertexMemory;

	std::shared_ptr<BottomLevelAccelerationStructure> BLAS;

	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;

	bool cullBackFaces = true;

	int faceCount = 0;
	glm::vec3 min, max, center, extents;

	float uvScale = 1.0f;

	void ResetMaterial(); // should make it so that it also deletes the material if no other mesh references it
	void ProcessMaterial(const MaterialCreationData& creationData);
	void Recreate();
	bool HasFinishedLoading() const;
	void AwaitGeneration();
	bool IsValid() const;

	uint32_t GetMaterialIndex() const;
	void SetMaterialIndex(uint32_t index);

	/// <summary>
	/// Sets the material for this mesh, any old mesh will be overridden.
	/// </summary>
	/// <param name="material"></param>
	void SetMaterial(const Material& material);

	Material& GetMaterial();

private:
	static uint32_t FindUnusedMaterial(); // returns the index to the material

	uint32_t materialIndex = 0;

	static std::mutex materialMutex;
	bool finished = false;
};