#include "Object.h"
#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <stdexcept>
#include <iostream>
#include <future>
#include "Console.h"
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

void Object::AwaitGeneration()
{
	generationProcess.get();
}

void Object::GenerateObjectWithData(const ObjectCreationObject& creationObject, const ObjectCreationData& creationData)
{
	name = creationData.name;

	#ifdef _DEBUG
	Console::WriteLine("Attempting to create new model \"" + name + '\"', MESSAGE_SEVERITY_DEBUG);
	#endif

	for (MeshCreationData meshData : creationData.meshes)
		meshes.push_back(Mesh{ creationObject, meshData });

	transform = Transform(creationData.position, creationData.rotation, creationData.scale, meshes[0].extents, meshes[0].center); // should determine the extents and center (minmax) of all meshes not just the first one
	hObject = ResourceManager::GenerateHandle();

	finishedLoading = true; //maybe use mutex here or just find better solution

	#ifdef _DEBUG
	Console::WriteLine("Created new object \"" + name + "\" with unique id \"" + std::to_string(hObject) + '\"', MESSAGE_SEVERITY_DEBUG);
	#endif
}

void Object::RecreateMeshes(const MeshCreationObject& creationObject)
{
	for (Mesh& mesh : meshes)
		mesh.Recreate(creationObject);
}
void Object::CreateObject(void* customClassPointer, const ObjectCreationData& creationData, const MeshCreationObject& creationObject)
{
	scriptClass = customClassPointer;
	GenerateObjectWithData(creationObject, creationData);
}

Object::Object(const ObjectCreationData& creationData, const ObjectCreationObject& creationObject)
{
	generationProcess = std::async(&Object::GenerateObjectWithData, this, creationObject, creationData);
}

bool Object::HasFinishedLoading()
{
	return finishedLoading || generationProcess._Is_ready();
}

void Object::Destroy()
{
	for (Mesh mesh : meshes)
		mesh.Destroy();
	delete this;
}