#include <algorithm>

#include "renderer/Mesh.h"

#include "io/CreationData.h"

#include "ResourceManager.h"

import Renderer.AccelerationStructure;
import Renderer;

std::vector<Material> Mesh::materials;
std::mutex Mesh::materialMutex;

Handle Mesh::AddMaterial(const Material& material)
{
	std::lock_guard<std::mutex> lockGuard(materialMutex);

	materials.push_back(material);

	Material& mat = materials.back();
	mat.handle = reinterpret_cast<Handle>(&mat);

	return mat.handle;
}

Handle Mesh::InsertMaterial(int index, const Material& material)
{
	std::lock_guard<std::mutex> lockGuard(materialMutex);

	Handle ret = 0;

	if (index < materials.size() && index >= 0)
	{
		Material& dst = materials[index];

		dst.Destroy();
		dst = material;
		ret = reinterpret_cast<Handle>(&dst);
		dst.handle = ret;
	}
	else
	{
		materials.push_back(material);
		Material& dst = materials.back();
		ret = ResourceManager::GenerateHandle();
		dst.handle = ret;
	}
	return ret;
}

void Mesh::ProcessMaterial(const MaterialCreationData& creationData)
{
	if (materialIndex != 0) // if this mesh already has a material, then dont replace that with this one
		return;

	Material mat = Material::Create(creationData);
	SetMaterial(mat);
}

void Mesh::Create(const MeshCreationData& creationData)
{
	vertices      = creationData.vertices;
	indices       = creationData.indices;
	faceCount     = creationData.faceCount;
	cullBackFaces = creationData.cullBackFaces;
	center        = (creationData.min + creationData.max) * 0.5f;
	extents       = creationData.max - center;
	max           = extents + center;
	min           = center * 2.f - max;
	flags         = static_cast<MeshFlags>(creationData.flags);

	SetMaterialIndex(creationData.materialIndex);

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
		BLAS = std::make_shared<BottomLevelAccelerationStructure>(*this);

	for (const Vertex& vertex : vertices) // better if this is precalculated
	{
		min = glm::min(vertex.position, min);
		max = glm::max(vertex.position, max);
		originalAABBDistance = std::max(originalAABBDistance, glm::length(vertex.position));
	}

	center = (min + max) * 0.5f;

	min = center - originalAABBDistance;
	max = center + originalAABBDistance;
}

void Mesh::CopyFrom(const Mesh& mesh)
{
	vertices = mesh.vertices;
	indices = mesh.indices;

	BLAS = mesh.BLAS;

	faceCount = mesh.faceCount;

	min = mesh.min;
	max = mesh.max;	
	center = mesh.center;
	extents = mesh.extents;

	finished = true;

	if (vertices.empty() || indices.empty())
		return;

	vertexMemory = mesh.vertexMemory;
	indexMemory = mesh.indexMemory;
	defaultVertexMemory = mesh.defaultVertexMemory;
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
		materialIndex = static_cast<std::uint32_t>(it - materials.begin());
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

uint32_t Mesh::GetMaterialIndex() const // the mesh will fall back to the default material if its actual material for some reason doesnt exist anymore
{
	return materialIndex >= materials.size() || !materials[materialIndex].HasFinishedLoading() ? 0 : materialIndex;
}

void Mesh::SetMaterialIndex(uint32_t index)
{
	materialIndex = index;
}

bool Mesh::HasFinishedLoading() const
{
	return finished;
}

void Mesh::AwaitGeneration()
{
	
}

bool Mesh::IsValid() const
{
	return !vertices.empty() && !indices.empty();
}

bool Mesh::CanBeRayTraced() const
{
	return !(flags & MESH_FLAG_NO_RAY_TRACING) && BLAS != nullptr;
}

MeshOptionFlags Mesh::GetFlags() const
{
	return flags;
}

void Mesh::SetFlags(MeshOptionFlags flags)
{
	if (this->flags == flags)
		return;

	if (flags & MESH_FLAG_NO_RAY_TRACING)
		BLAS.reset();
	else
		BLAS = std::make_shared<BottomLevelAccelerationStructure>(*this);

	this->flags = flags;
}

void Mesh::Destroy()
{
	if (materialIndex < materials.size())
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

	return static_cast<std::uint32_t>(it - materials.begin());
}

void Mesh::UpdateMinMax(const glm::vec3& translation, const glm::vec3& scale)
{
	glm::vec3 extents = originalAABBDistance * glm::vec3(glm::compMax(scale));

	min = translation + center - extents;
	max = translation + center + extents;
}