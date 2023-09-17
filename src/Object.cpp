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

/*Mesh::Mesh(const MeshCreationObject& creationObject, aiMesh* mesh, aiMaterial* material)
{
	//ProcessMaterial(creationObject, material);
	ProcessIndices(mesh);
	//ProcessVertices(mesh);
	Recreate(creationObject);
}*/

/*aiString GetTexturePath(aiMaterial* material, aiTextureType textureType)
{
	aiString path;
	aiReturn ret = material->GetTexture(textureType, 0, &path);
	if (ret == -1)
		throw std::runtime_error("A given object doesn't have a required texture");
	return path;
}

void Mesh::ProcessIndices(aiMesh* mesh)
{
	for (int i = 0; i < mesh->mNumFaces; i++)
		for (int j = 0; j < mesh->mFaces[i].mNumIndices; j++)
			indices.push_back(mesh->mFaces[i].mIndices[j]);
}*/

/*glm::vec3 ConvertToGlm(aiVector3D vector)
{
	glm::vec3 ret;
	ret.x = vector.x;
	ret.y = vector.y;
	ret.z = vector.z;
	return ret;
}

glm::vec2 ConverToGlm(aiVector3D vector)
{
	glm::vec2 ret;
	ret.x = vector.x;
	ret.y = vector.y;
	return ret;
}

void Mesh::ProcessVertices(aiMesh* mesh)
{
	for (int i = 0; i < mesh->mNumVertices; i++)
	{
		Vertex vertex{};

		vertex.position = ConvertToGlm(mesh->mVertices[i]);

		max.x = vertex.position.x > max.x ? vertex.position.x : max.x;
		max.y = vertex.position.y > max.y ? vertex.position.y : max.y;
		max.z = vertex.position.z > max.z ? vertex.position.z : max.z;

		min.x = vertex.position.x < min.x ? vertex.position.x : min.x;
		min.y = vertex.position.y < min.y ? vertex.position.y : min.y;
		min.z = vertex.position.z < min.z ? vertex.position.z : min.z;

		if (mesh->HasNormals())
			vertex.normal = ConvertToGlm(mesh->mNormals[i]);
		if (mesh->mTextureCoords[0])
			vertex.textureCoordinates = ConverToGlm(mesh->mTextureCoords[0][i]);

		vertices.push_back(vertex);
	}
	center = (min + max) * 0.5f;
	extents = max - center;
}*/

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
	ObjectCreationData creationData = GenericLoader::LoadObjectFile(path);
	GenerateObjectWithData(object, creationObject, creationData);

	/*// this takes the filename out of the full path by indexing the \'s and the file extension
	std::string fileNameWithExtension = path.substr(path.find_last_of("/\\") + 1);
	object->name = fileNameWithExtension.substr(0, fileNameWithExtension.find_last_of('.'));

	const aiScene* scene = aiImportFile(path.c_str(), aiProcess_FixInfacingNormals | aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_RemoveComponent | aiProcess_GenSmoothNormals);

	if (scene == nullptr) // check if the file could be read
		throw std::runtime_error("Failed to find or read file at " + path);

	for (int i = 0; i < scene->mNumMeshes; i++) // convert the assimp resources into the engines resources
	{
		aiMesh* pMesh = scene->mMeshes[i];
		aiMaterial* material = scene->mMaterials[pMesh->mMaterialIndex];
		object->meshes.push_back(Mesh{ creationObject, pMesh, material });
	}
	object->transform = Transform(glm::vec3(0), glm::vec3(0), glm::vec3(1), object->meshes[0].extents, object->meshes[0].center); // should determine the extents and center (minmax) of all meshes not just the first one

	GenerateUUID(&object->uuid);

	object->finishedLoading = true; //maybe use mutex here or just find better solution

	#ifdef _DEBUG
		char* str;
		UuidToStringA(&object->uuid, (RPC_CSTR*)&str);
		Console::WriteLine("Created new object \"" + object->name + "\" with unique id \"" + str + '\"', MESSAGE_SEVERITY_DEBUG);
	#endif*/
}

void Object::RecreateMeshes(const MeshCreationObject& creationObject)
{
	for (Mesh& mesh : meshes)
		mesh.Recreate(creationObject);
}
/*
void Object::CreateObject(void* customClassPointer, const ObjectCreationData& creationData, const MeshCreationObject& creationObject)
{
	scriptClass = customClassPointer;
	GenerateObjectWithData(this, creationObject, creationData);
	finishedLoading = true;
}
*/
void Object::CreateObject/*Async*/(void* customClassPointer, const ObjectCreationData& creationData, const MeshCreationObject& creationObject)
{
	scriptClass = customClassPointer;
	GenerateObjectWithData(this, creationObject, creationData);//generationProcess = std::async(GenerateObjectWithData, this, creationObject, creationData);
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