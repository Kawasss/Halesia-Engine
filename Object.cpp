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

Mesh::Mesh(VkDevice logicalDevice, PhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue, aiMesh* mesh)
{
	ProcessIndices(mesh);
	ProcessVertices(mesh);
	vertexBuffer = VertexBuffer(logicalDevice, physicalDevice, commandPool, queue, vertices);
	indexBuffer = IndexBuffer(logicalDevice, physicalDevice, commandPool, queue, indices);
}

void Mesh::Destroy()
{
	vertexBuffer.Destroy();
	indexBuffer.Destroy();
	delete this;
}

void Mesh::ProcessIndices(aiMesh* mesh)
{
	for (int j = 0; j < mesh->mNumFaces; j++)
		for (int i = 0; i < mesh->mFaces[j].mNumIndices; i++)
			indices.push_back(mesh->mFaces[j].mIndices[i]);
}

void Mesh::ProcessVertices(aiMesh* mesh)
{
	for (int i = 0; i < mesh->mNumVertices; i++)
	{
		Vertex vertex{};

		vertex.position.x = mesh->mVertices[i].x;
		vertex.position.y = mesh->mVertices[i].y;
		vertex.position.z = mesh->mVertices[i].z;

		max.x = vertex.position.x > max.x ? vertex.position.x : max.x;
		max.y = vertex.position.y > max.y ? vertex.position.y : max.y;
		max.z = vertex.position.z > max.z ? vertex.position.z : max.z;

		min.x = vertex.position.x < min.x ? vertex.position.x : min.x;
		min.y = vertex.position.y < min.y ? vertex.position.y : min.y;
		min.z = vertex.position.z < min.z ? vertex.position.z : min.z;

		if (mesh->HasNormals())
		{
			vertex.normal.x = mesh->mNormals[i].x;
			vertex.normal.y = mesh->mNormals[i].y;
			vertex.normal.z = mesh->mNormals[i].z;
		}
		if (mesh->mTextureCoords[0])
		{
			vertex.textureCoordinates.x = mesh->mTextureCoords[0][i].x;
			vertex.textureCoordinates.y = mesh->mTextureCoords[0][i].y;
		}
		vertices.push_back(vertex);
	}
	center = (min + max) * 0.5f;
	extents = max - center;
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

void GenerateObject(Object* object, VkDevice logicalDevice, PhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue, std::string path)
{
	// this take the filename out of the full path by indexing the \'s and the file extension
	std::string fileNameWithExtension = path.substr(path.find_last_of("/\\") + 1);
	object->name = fileNameWithExtension.substr(0, fileNameWithExtension.find_last_of('.'));

	const aiScene* scene = aiImportFile(path.c_str(), aiProcessPreset_TargetRealtime_Fast);

	if (scene == nullptr) // check if the file could be read
		throw std::runtime_error("Failed to find or read file at " + path);
	
	for (int i = 0; i < scene->mNumMeshes; i++) // convert the assimp resources into the engines resources
	{
		aiMesh* pMesh = scene->mMeshes[i];
		object->meshes.push_back(Mesh{ logicalDevice, physicalDevice, commandPool, queue, pMesh });
	}
	object->transform = Transform(glm::vec3(0), glm::vec3(0), glm::vec3(1), object->meshes[0].extents, object->meshes[0].center); // should determine the extents and center (minmax) of all meshes not just the first one

	GenerateUUID(&object->uuid);

	object->finishedLoading = true; //maybe use mutex here or just find better solution

	#ifdef _DEBUG
		char* str;

		UuidToStringA(&object->uuid, (RPC_CSTR*)&str);
		Console::WriteLine("Created new object \"" + object->name + "\" with unique id \"" + str + '\"');
	#endif
}

void Object::CreateObject(void* customClassPointer, std::string path, const MeshCreationObjects& creationObjects)
{
	scriptClass = customClassPointer;
	GenerateObject(this, creationObjects.logicalDevice, creationObjects.physicalDevice, creationObjects.commandPool, creationObjects.queue, path);
	finishedLoading = true;
}

void Object::CreateObjectAsync(void* customClassPointer, std::string path, const MeshCreationObjects& creationObjects)
{
	scriptClass = customClassPointer;
	generationProcess = std::async(GenerateObject, this, creationObjects.logicalDevice, creationObjects.physicalDevice, creationObjects.commandPool, creationObjects.queue, path);
}

Object::Object(std::string path, const MeshCreationObjects& creationObjects)
{
	CreateObjectAsync(nullptr, path, creationObjects);
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