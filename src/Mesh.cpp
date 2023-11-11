#include <algorithm>
#include "renderer/Mesh.h"
#include "renderer/Renderer.h"
#include "renderer/AccelerationStructures.h"
#include "SceneLoader.h"

std::vector<Material> Mesh::materials;

void Mesh::ProcessMaterial(const TextureCreationObject& creationObjects, const MaterialCreationData& creationData)
{
	if (materialIndex != 0) // if this mesh already has a material, then dont replace that with this one
		return;

	Texture* albedo = !creationData.albedoIsDefault ? new Texture(creationObjects, creationData.albedoData) : Texture::placeholderAlbedo;
	Texture* normal = !creationData.normalIsDefault ? new Texture(creationObjects, creationData.normalData) : Texture::placeholderNormal;
	Texture* metallic = !creationData.metallicIsDefault ? new Texture(creationObjects, creationData.metallicData) : Texture::placeholderMetallic;
	Texture* roughness = !creationData.roughnessIsDefault ? new Texture(creationObjects, creationData.roughnessData) : Texture::placeholderRoughness;
	Texture* ambientOcclusion = !creationData.ambientOcclusionIsDefault ? new Texture(creationObjects, creationData.ambientOcclusionData) : Texture::placeholderAmbientOcclusion;
	SetMaterial({ albedo, normal, metallic, roughness, ambientOcclusion });
}

void Mesh::Create(const MeshCreationObject& creationObject, const MeshCreationData& creationData)
{
	if (materials.size() == 0)
		materials.push_back({ Texture::placeholderAlbedo, Texture::placeholderNormal, Texture::placeholderMetallic, Texture::placeholderRoughness, Texture::placeholderAmbientOcclusion });

	vertices = creationData.vertices;
	indices = creationData.indices;
	faceCount = creationData.faceCount;
	if (creationData.hasMaterial)
		ProcessMaterial(creationObject, creationData.material);
	Recreate(creationObject);
}

void Mesh::Recreate(const MeshCreationObject& creationObject)
{
	vertexMemory = Renderer::globalVertexBuffer.SubmitNewData(vertices);
	indexMemory = Renderer::globalIndicesBuffer.SubmitNewData(indices);
	BLAS = BottomLevelAccelerationStructure::Create(creationObject, *this);
}

void Mesh::SetMaterial(Material material)
{
	std::vector<Material>::iterator materialLocationInGlobalVector = std::find(materials.begin(), materials.end(), material); // make sure that a material is only submitted once
	if (materialLocationInGlobalVector < materials.end())
	{
		materialIndex = materials.begin() - materialLocationInGlobalVector;
		return;
	}

	materials.push_back(material); // maybe make it so that the previous material is removed and destroyed if no other meshes references it
	for (int i = 0; i < materials.size(); i++) // poor performance
		if (materials[i] == material)
			materialIndex = i;
}

bool Mesh::HasFinishedLoading()
{
	return materials[materialIndex].HasFinishedLoading();
}

void Mesh::Destroy()
{
	// should also delete the material in materials here (if no other meshes are referencing that material)
	BLAS->Destroy();
	Renderer::globalVertexBuffer.DestroyData(vertexMemory);
	Renderer::globalIndicesBuffer.DestroyData(indexMemory);
	indices.clear();
	vertices.clear();
	//delete this;
}