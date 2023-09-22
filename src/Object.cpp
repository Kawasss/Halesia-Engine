#pragma comment(lib, "rpcrt4.lib")
#include "Object.h"
#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <stdexcept>
#include <rpc.h>
#include <winerror.h>
#include <iostream>
#include <future>
#include "Console.h"

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
	if (creationData.hasMaterial)
		ProcessMaterial(creationObject, creationData.material);
	Recreate(creationObject);
}

void Mesh::Recreate(const MeshCreationObject& creationObject)
{
	vertexBuffer = VertexBuffer(creationObject, vertices);
	indexBuffer = IndexBuffer(creationObject, indices);
}

void Mesh::Destroy()
{
	material.Destroy();
	vertexBuffer.Destroy();
	indexBuffer.Destroy();
	delete this;
}

void GenerateUUID(UUID* uuid)
{
	if (HRESULT_FROM_WIN32(UuidCreate(uuid)) != S_OK)
		throw std::runtime_error("Failed to create a UUID for its parent object");
}

void Object::AwaitGeneration()
{
	generationProcess.get();
}

void GenerateObjectWithData(Object* object, ObjectCreationObject creationObject, ObjectCreationData creationData)
{
	object->name = creationData.name;

#ifdef _DEBUG
	Console::WriteLine("Attempting to create new model \"" + object->name + '\"', MESSAGE_SEVERITY_DEBUG);
#endif

	for (MeshCreationData meshData : creationData.meshes)
		object->meshes.push_back(Mesh{ creationObject, meshData });

	object->transform = Transform(creationData.position, creationData.rotation, creationData.scale, object->meshes[0].extents, object->meshes[0].center); // should determine the extents and center (minmax) of all meshes not just the first one
	GenerateUUID(&object->uuid);

	object->finishedLoading = true; //maybe use mutex here or just find better solution

#ifdef _DEBUG
	char* str;
	UuidToStringA(&object->uuid, (RPC_CSTR*)&str);
	Console::WriteLine("Created new object \"" + object->name + "\" with unique id \"" + str + '\"', MESSAGE_SEVERITY_DEBUG);
#endif
}

void GenerateObject(Object* object, const ObjectCreationObject& creationObject, std::string path)
{
	//ObjectCreationData creationData = GenericLoader::LoadObjectFile(path);
	//GenerateObjectWithData(object, creationObject, creationData);
}

void Object::RecreateMeshes(const MeshCreationObject& creationObject)
{
	for (Mesh& mesh : meshes)
		mesh.Recreate(creationObject);
}
void Object::CreateObject(void* customClassPointer, const ObjectCreationData& creationData, const MeshCreationObject& creationObject)
{
	scriptClass = customClassPointer;
	GenerateObjectWithData(this, creationObject, creationData);
}

void Object::CreateObject(void* customClassPointer, std::string path, const MeshCreationObject& creationObject)
{
	scriptClass = customClassPointer;
	generationProcess = std::async(GenerateObject, this, creationObject, path);
}

Object::Object(const ObjectCreationData& creationData, const ObjectCreationObject& creationObject)
{
	generationProcess = std::async(GenerateObjectWithData, this, creationObject, creationData);
}

Object::Object(std::string path, const ObjectCreationObject& creationObject)
{
	generationProcess = std::async(GenerateObject, this, creationObject, path);
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