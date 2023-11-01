#include "renderer/Mesh.h"
#include "renderer/Renderer.h"

void Mesh::ProcessMaterial(const TextureCreationObject& creationObjects, const MaterialCreationData& creationData)
{
	Texture* albedo = !creationData.albedoIsDefault ? new Texture(creationObjects, creationData.albedoData) : Texture::placeholderAlbedo;
	Texture* normal = !creationData.normalIsDefault ? new Texture(creationObjects, creationData.normalData) : Texture::placeholderNormal;
	Texture* metallic = !creationData.metallicIsDefault ? new Texture(creationObjects, creationData.metallicData) : Texture::placeholderMetallic;
	Texture* roughness = !creationData.roughnessIsDefault ? new Texture(creationObjects, creationData.roughnessData) : Texture::placeholderRoughness;
	Texture* ambientOcclusion = !creationData.ambientOcclusionIsDefault ? new Texture(creationObjects, creationData.ambientOcclusionData) : Texture::placeholderAmbientOcclusion;
	this->material = { albedo, normal, metallic, roughness, ambientOcclusion };
}

Mesh::Mesh(const MeshCreationObject& creationObject, const MeshCreationData& creationData)
{
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

void Mesh::Destroy()
{
	material.Destroy();
	BLAS->Destroy();
	Renderer::globalVertexBuffer.DestroyData(vertexMemory);
	Renderer::globalIndicesBuffer.DestroyData(indexMemory);
	indices.clear();
	vertices.clear();
	//delete this;
}