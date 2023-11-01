#include "renderer/Mesh.h"
#include "renderer/Renderer.h"

std::vector<Material> Mesh::materials;

void Mesh::ProcessMaterial(const TextureCreationObject& creationObjects, const MaterialCreationData& creationData)
{
	Texture* albedo = !creationData.albedoIsDefault ? new Texture(creationObjects, creationData.albedoData) : Texture::placeholderAlbedo;
	Texture* normal = !creationData.normalIsDefault ? new Texture(creationObjects, creationData.normalData) : Texture::placeholderNormal;
	Texture* metallic = !creationData.metallicIsDefault ? new Texture(creationObjects, creationData.metallicData) : Texture::placeholderMetallic;
	Texture* roughness = !creationData.roughnessIsDefault ? new Texture(creationObjects, creationData.roughnessData) : Texture::placeholderRoughness;
	Texture* ambientOcclusion = !creationData.ambientOcclusionIsDefault ? new Texture(creationObjects, creationData.ambientOcclusionData) : Texture::placeholderAmbientOcclusion;
	this->materialIndex = materials.size(); // the newest material is always at the end of the buffer, so its index is the size of the buffer
	materials.push_back({ albedo, normal, metallic, roughness, ambientOcclusion });
}

Mesh::Mesh(const MeshCreationObject& creationObject, const MeshCreationData& creationData)
{
	if (materials.size() == 0)
		materials.push_back({ Texture::placeholderAlbedo, Texture::placeholderNormal, Texture::placeholderMetallic, Texture::placeholderRoughness, Texture::placeholderAmbientOcclusion });

	vertices = creationData.vertices;
	indices = creationData.indices;
	faceCount = creationData.faceCount;
	if (creationData.hasMaterial)
		ProcessMaterial(creationObject, creationData.material);
	else materialIndex = 0;
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
	// should also delete the material in materials here (if no other meshes are referencing that material)
	BLAS->Destroy();
	Renderer::globalVertexBuffer.DestroyData(vertexMemory);
	Renderer::globalIndicesBuffer.DestroyData(indexMemory);
	indices.clear();
	vertices.clear();
	//delete this;
}