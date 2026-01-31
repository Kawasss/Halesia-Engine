module;

#include "Material.h"

export module Renderer.Mesh;

import std;

import "../glm.h";

import Renderer.Vertex;

import IO.CreationData;

export enum MeshFlags
{
	MeshFlagNone = 0,
	MeshFlagNoRayTracing = 1 << 1,
	MeshFlagCullBackFaces = 1 << 2,
};
export using MeshOptionFlags = std::underlying_type_t<MeshFlags>;

using MeshHandle = std::uintptr_t;

export struct Mesh
{
	static Handle AddMaterial(const Material& material); // returns the handle to the material
	static Handle InsertMaterial(int index, const Material& material);

	static std::vector<Material> materials;

	void Create(const MeshCreationData& creationData);
	void Destroy();

	void CopyFrom(const Mesh& mesh);

	MeshHandle meshHandle = 0;

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
	bool CanBeRayTraced() const;

	MeshOptionFlags GetFlags() const;
	void SetFlags(MeshOptionFlags flags);

	uint32_t GetMaterialIndex() const;
	void SetMaterialIndex(uint32_t index);

	void UpdateMinMax(const glm::vec3& translation, const glm::vec3& scale);

	/// <summary>
	/// Sets the material for this mesh, any old mesh will be overridden.
	/// </summary>
	/// <param name="material"></param>
	void SetMaterial(const Material& material);

	Material& GetMaterial();

private:
	static uint32_t FindUnusedMaterial(); // returns the index to the material

	uint32_t materialIndex = 0;

	MeshOptionFlags flags = MeshFlagNone;

	float originalAABBDistance = 0.0f;

	static std::mutex materialMutex;
	bool finished = false;
};