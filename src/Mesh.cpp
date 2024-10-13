#include <algorithm>

#include "renderer/Mesh.h"
#include "renderer/Renderer.h"
#include "renderer/AccelerationStructures.h"

#include "io/SceneLoader.h"

std::vector<Material> Mesh::materials;
std::mutex Mesh::materialMutex;

void Mesh::AddMaterial(const Material& material)
{
	materials.push_back(material);
	
	Material& mat = materials.back();
	mat.handle = reinterpret_cast<Handle>(&mat);
}

void Mesh::ProcessMaterial(const MaterialCreationData& creationData)
{
	if (materialIndex != 0) // if this mesh already has a material, then dont replace that with this one
		return;

	Texture* albedo    = !creationData.albedoData.empty()           ? new Texture(creationData.albedoData, creationData.aWidth, creationData.aHeight) : Texture::placeholderAlbedo;
	Texture* normal    = !creationData.normalData.empty()           ? new Texture(creationData.normalData, creationData.nWidth, creationData.nHeight) : Texture::placeholderNormal;
	Texture* metallic  = !creationData.metallicData.empty()         ? new Texture(creationData.metallicData, creationData.mWidth, creationData.mHeight) : Texture::placeholderMetallic;
	Texture* roughness = !creationData.roughnessData.empty()        ? new Texture(creationData.roughnessData, creationData.rWidth, creationData.rHeight) : Texture::placeholderRoughness;
	Texture* ao        = !creationData.ambientOcclusionData.empty() ? new Texture(creationData.ambientOcclusionData, creationData.aoWidth, creationData.aoHeight) : Texture::placeholderAmbientOcclusion;
	SetMaterial({ albedo, normal, metallic, roughness, ao });
}

void Mesh::Create(const MeshCreationData& creationData)
{
	name      = creationData.name;
	vertices  = creationData.vertices;
	indices   = creationData.indices;
	faceCount = creationData.faceCount;
	center    = creationData.center;
	extents   = creationData.extents;
	max       = extents + center;
	min       = center * 2.f - max;
	materialIndex = creationData.materialIndex;

	Recreate();
	finished = true;
}

void Mesh::Recreate()
{
	if (vertexMemory != 0)
	{
		Renderer::g_vertexBuffer.DestroyData(vertexMemory);
		Renderer::g_defaultVertexBuffer.DestroyData(defaultVertexMemory);
		Renderer::g_indexBuffer.DestroyData(indexMemory);
	}

	vertexMemory        = Renderer::g_vertexBuffer.SubmitNewData(vertices);
	indexMemory         = Renderer::g_indexBuffer.SubmitNewData(indices);
	defaultVertexMemory = Renderer::g_defaultVertexBuffer.SubmitNewData(vertices);

	if (Renderer::canRayTrace)
		BLAS = BottomLevelAccelerationStructure::Create(*this);
}

void Mesh::ResetMaterial()
{
	materialIndex = 0;
}

void Mesh::SetMaterial(const Material& material)
{
	std::lock_guard<std::mutex> lockGuard(materialMutex);

	auto it = std::find(materials.begin(), materials.end(), material);
	if (it != materials.end())
	{
		materialIndex = it - materials.begin();
	}
	else
	{
		materialIndex = FindUnusedMaterial(); // try to find an open spot to prevent a new allocation and to save space
		if (materialIndex != materials.size())
		{
			materials[materialIndex] = material;
		}
		else
		{
			AddMaterial(material);
		}
	}
	materials[materialIndex].AddReference();
}

Material& Mesh::GetMaterial()
{
	return materials[GetMaterialIndex()];
}

uint32_t Mesh::GetMaterialIndex() // the mesh will fall back to the default material if its actual material for some reason doesnt exist anymore
{
	if (materialIndex >= materials.size())
		materialIndex = 0;
	return materialIndex;
}

bool Mesh::HasFinishedLoading() const
{
	return materials[materialIndex].HasFinishedLoading() && finished;
}

void Mesh::AwaitGeneration()
{
	materials[materialIndex].AwaitGeneration();
}

bool Mesh::IsValid() const
{
	return !vertices.empty() && !indices.empty();
}

void Mesh::Destroy()
{
	materials[materialIndex].RemoveReference();

	// should also delete the material in materials here (if no other meshes are referencing that material)
	if (vertexMemory != 0)
		Renderer::g_vertexBuffer.DestroyData(vertexMemory);
	if (indexMemory != 0)
		Renderer::g_indexBuffer.DestroyData(indexMemory);
	if (defaultVertexMemory != 0)
		Renderer::g_defaultVertexBuffer.DestroyData(defaultVertexMemory);
	indices.clear();
	vertices.clear();
	//delete this;
}

uint32_t Mesh::FindUnusedMaterial() // a material is unused if it is not the default material and all textures are the default texture
{
	const Material& unusedMaterialTemplate = materials.front(); // the default material (which is at the front) will compare true to an unused material
	auto it = std::find(materials.begin() + 1, materials.end(), unusedMaterialTemplate);

	return it - materials.begin();
}