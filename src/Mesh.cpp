#include <algorithm>
#include "renderer/Mesh.h"
#include "renderer/Renderer.h"
#include "renderer/AccelerationStructures.h"
#include "io/SceneLoader.h"

std::vector<Material> Mesh::materials;
std::mutex Mesh::materialMutex;

void Mesh::ProcessMaterial(const MaterialCreationData& creationData)
{
	if (materialIndex != 0) // if this mesh already has a material, then dont replace that with this one
		return;

	Texture* albedo = !creationData.albedoIsDefault ? new Texture(creationData.albedoData) : Texture::placeholderAlbedo;
	Texture* normal = !creationData.normalIsDefault ? new Texture(creationData.normalData) : Texture::placeholderNormal;
	Texture* metallic = !creationData.metallicIsDefault ? new Texture(creationData.metallicData) : Texture::placeholderMetallic;
	Texture* roughness = !creationData.roughnessIsDefault ? new Texture(creationData.roughnessData) : Texture::placeholderRoughness;
	Texture* ambientOcclusion = !creationData.ambientOcclusionIsDefault ? new Texture(creationData.ambientOcclusionData) : Texture::placeholderAmbientOcclusion;
	SetMaterial({ albedo, normal, metallic, roughness, ambientOcclusion });
}

void Mesh::Create(const MeshCreationData& creationData)
{
	if (materials.size() == 0)
		materials.push_back({ Texture::placeholderAlbedo, Texture::placeholderNormal, Texture::placeholderMetallic, Texture::placeholderRoughness, Texture::placeholderAmbientOcclusion });

	name = creationData.name;
	vertices = creationData.vertices;
	indices = creationData.indices;
	faceCount = creationData.faceCount;
	center = creationData.center;
	extents = creationData.extents;
	max = extents + center;
	min = center * 2.f - max;
	materialIndex = creationData.materialIndex;

	Recreate();
	finished = true;
}

void Mesh::Recreate()
{
	vertexMemory = Renderer::g_vertexBuffer.SubmitNewData(vertices);
	indexMemory = Renderer::g_indexBuffer.SubmitNewData(indices);
	defaultVertexMemory = Renderer::g_defaultVertexBuffer.SubmitNewData(vertices);
	BLAS = BottomLevelAccelerationStructure::Create(*this);
}

void Mesh::ResetMaterial()
{
	materialIndex = 0;
}

void Mesh::SetMaterial(Material material)
{
	std::lock_guard<std::mutex> lockGuard(materialMutex);
	for (int i = 0; i < materials.size(); i++) // this code is bad and should not stay for long, but it currently prevents a bug related to material uploading
		if (materials[i] == material)
		{
			materialIndex = i;
			return;
		}

	materials.push_back(material); // maybe make it so that the previous material is removed and destroyed if no other meshes references it
	for (int i = 0; i < materials.size(); i++) // poor performance
		if (materials[i] == material)
			materialIndex = i;
}

bool Mesh::HasFinishedLoading()
{
	return materials[materialIndex].HasFinishedLoading() && finished;
}

void Mesh::AwaitGeneration()
{
	materials[materialIndex].AwaitGeneration();
}

void Mesh::Destroy()
{
	// should also delete the material in materials here (if no other meshes are referencing that material)
	//BLAS->Destroy();
	Renderer::g_vertexBuffer.DestroyData(vertexMemory);
	Renderer::g_indexBuffer.DestroyData(indexMemory);
	Renderer::g_defaultVertexBuffer.DestroyData(defaultVertexMemory);
	indices.clear();
	vertices.clear();
	//delete this;
}