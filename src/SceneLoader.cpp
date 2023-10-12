#include "SceneLoader.h"
#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

SceneLoader::SceneLoader(std::string sceneLocation)
{
	this->location = sceneLocation;
}

void SceneLoader::LoadScene()
{
	OpenInputFile(location);
	RetrieveHeader();
	RetrieveCameraVariables();
	RetrieveLightVariables();
	RetrieveObjectVariables();
	RetrieveAllObjects();
}

glm::vec3 SceneLoader::GetVec3(char* bytes)
{
	glm::vec3 ret;
	ret.x = GetType<float>(bytes);
	ret.y = GetType<float>(bytes + 4);
	ret.z = GetType<float>(bytes + 8);
	return ret;
}

glm::vec2 SceneLoader::GetVec2(char* bytes)
{
	glm::vec2 ret;
	ret.x = GetType<float>(bytes);
	ret.y = GetType<float>(bytes + 4);
	return ret;
}

std::string SceneLoader::RetrieveName()
{
	char nameBytes[11];
	stream.read(nameBytes, 10);
	nameBytes[10] = '\0';
	return (std::string)nameBytes;
}

glm::vec3 SceneLoader::RetrieveTransformData()
{
	char posBytes[12];
	stream.read(posBytes, 12);
	return GetVec3(posBytes);
}

void SceneLoader::RetrieveTexture(std::vector<char>& vectorToWriteTo, bool& isDefault)
{
	char isDefaultByte;
	stream.read(&isDefaultByte, 1);
	isDefault = GetType<int8_t>(&isDefaultByte);
	if (isDefault)
		return;

	char lengthBytes[4];
	stream.read(lengthBytes, 4);
	
	int textureLength = GetType<int>(lengthBytes);
	if (textureLength <= 0)
	{
		isDefault = true;
		return;
	}
	vectorToWriteTo = std::vector<char>(textureLength);
	stream.read(vectorToWriteTo.data(), textureLength);
}

void SceneLoader::RetrieveOneMaterial(MaterialCreationData& creationData)
{
	RetrieveTexture(creationData.albedoData, creationData.albedoIsDefault);
	RetrieveTexture(creationData.normalData, creationData.normalIsDefault);
	RetrieveTexture(creationData.metallicData, creationData.metallicIsDefault);
	RetrieveTexture(creationData.roughnessData, creationData.roughnessIsDefault);
	RetrieveTexture(creationData.ambientOcclusionData, creationData.ambientOcclusionIsDefault);
	RetrieveTexture(creationData.heightData, creationData.heightIsDefault);
}

Vertex SceneLoader::RetrieveOneVertex()
{
	Vertex ret;
	char vecBytes[32];
	stream.read(vecBytes, 32);
	ret.position = GetVec3(vecBytes);
	ret.textureCoordinates = GetVec2(vecBytes + 12);
	ret.normal = GetVec3(vecBytes + 20);
	ret.drawID = 0;
	return ret;
}

void SceneLoader::RetrieveOneMesh(MeshCreationData& creationData)
{
	creationData.name = RetrieveName();
	for (int i = 0; i < 3; i++) // the file contains the transform data of the meshes, but those are already processed into the objects transformation, so ignore these
		RetrieveTransformData();

	char containsBonesOrTextures;
	stream.read(&containsBonesOrTextures, 1);
	creationData.hasMaterial = GetType<int8_t>(&containsBonesOrTextures);
	stream.read(&containsBonesOrTextures, 1);
	creationData.hasBones = GetType<int8_t>(&containsBonesOrTextures);
	
	char amountOfVerticesBytes[4];
	stream.read(amountOfVerticesBytes, 4);
	creationData.amountOfVertices = GetType<int>(amountOfVerticesBytes) * 3;
	
	glm::vec3 max = glm::vec3(0), min = glm::vec3(0);
	for (int i = 0; i < creationData.amountOfVertices; i++)
	{
		Vertex vertex = RetrieveOneVertex();

		max.x = vertex.position.x > max.x ? vertex.position.x : max.x;
		max.y = vertex.position.y > max.y ? vertex.position.y : max.y;
		max.z = vertex.position.z > max.z ? vertex.position.z : max.z;

		min.x = vertex.position.x < min.x ? vertex.position.x : min.x;
		min.y = vertex.position.y < min.y ? vertex.position.y : min.y;
		min.z = vertex.position.z < min.z ? vertex.position.z : min.z;

		creationData.vertices.push_back(vertex); // could do resize like in RetrieveOneObject, but one mesh can have thousands of vertices so i dont know about the copying speed of that
		creationData.indices.push_back(i); // since CRS doesnt support indices (yet), this is filled like there are no indices
	}
	creationData.center = (min + max) * 0.5f;
	creationData.extents = max - creationData.center;
	
	if (creationData.hasMaterial)
		RetrieveOneMaterial(creationData.material);
}

void SceneLoader::RetrieveOneObject(int index)
{
	ObjectCreationData& creationData = objects[index];

	creationData.name = RetrieveName();
	creationData.position = RetrieveTransformData();
	creationData.scale = RetrieveTransformData();
	creationData.rotation = RetrieveTransformData();
	
	char amountOfMeshesBytes[4];
	stream.read(amountOfMeshesBytes, 4);
	
	creationData.amountOfMeshes = GetType<int>(amountOfMeshesBytes);
	creationData.meshes.resize(creationData.amountOfMeshes);
	
	for (int i = 0; i < creationData.amountOfMeshes; i++)
		RetrieveOneMesh(creationData.meshes[i]);
}

void SceneLoader::RetrieveAllObjects()
{
	for (int i = 0; i < amountOfObjects; i++)
		RetrieveOneObject(i);
}

void SceneLoader::RetrieveObjectVariables()
{
	char amountBytes[4];
	stream.read(amountBytes, 4);
	amountOfObjects = GetType<int>(amountBytes);

	objects.resize(amountOfObjects);
}

void SceneLoader::RetrieveCameraVariables()
{
	char cameraBytes[20];
	stream.read(cameraBytes, 20);
	cameraPos = GetVec3(cameraBytes);
	cameraPitch = GetType<float>(cameraBytes + 12);
	cameraYaw = GetType<float>(cameraBytes + 16);
}

void SceneLoader::RetrieveLightVariables()
{
	char lightBytes[12];
	stream.read(lightBytes, 12);
	lightPos = GetVec3(lightBytes);
}

void SceneLoader::RetrieveHeader()
{
	char lHeader[101];
	stream.read(lHeader, 100);
	lHeader[100] = '\0';
	this->header = std::string(lHeader);
}

void SceneLoader::OpenInputFile(std::string path)
{
	stream.open(path, std::ios::in | std::ios::binary);
	if (!stream)
		throw std::runtime_error("Failed to open the scene file at " + path + ", the path is either invalid or outdated / corrupt");
}

std::vector<Vertex> RetrieveVertices(aiMesh* pMesh, glm::vec3& min, glm::vec3& max, int index)
{
	std::vector<Vertex> ret;
	for (int i = 0; i < pMesh->mNumVertices; i++)
	{
		Vertex vertex{};

		vertex.position = glm::vec3(pMesh->mVertices[i].x, pMesh->mVertices[i].y, pMesh->mVertices[i].z);

		max.x = vertex.position.x > max.x ? vertex.position.x : max.x;
		max.y = vertex.position.y > max.y ? vertex.position.y : max.y;
		max.z = vertex.position.z > max.z ? vertex.position.z : max.z;

		min.x = vertex.position.x < min.x ? vertex.position.x : min.x;
		min.y = vertex.position.y < min.y ? vertex.position.y : min.y;
		min.z = vertex.position.z < min.z ? vertex.position.z : min.z;

		if (pMesh->HasNormals())
			vertex.normal = glm::vec3(pMesh->mNormals[i].x, pMesh->mNormals[i].y, pMesh->mNormals[i].z);
		if (pMesh->mTextureCoords[0])
			vertex.textureCoordinates = glm::vec2(pMesh->mTextureCoords[0][i].x, pMesh->mTextureCoords[0][i].y);
		vertex.drawID = index;
		ret.push_back(vertex);
	}
	return ret;
}

std::vector<uint16_t> RetrieveIndices(aiMesh* pMesh)
{
	std::vector<uint16_t> ret;
	for (int i = 0; i < pMesh->mNumFaces; i++)
		for (int j = 0; j < pMesh->mFaces[i].mNumIndices; j++)
			ret.push_back(pMesh->mFaces[i].mIndices[j]);
	return ret;
}

MeshCreationData RetrieveMeshData(aiMesh* pMesh, int index)
{
	MeshCreationData ret{};

	glm::vec3 min = glm::vec3(0), max = glm::vec3(0);
	ret.amountOfVertices = pMesh->mNumVertices;
	ret.vertices = RetrieveVertices(pMesh, min, max, index);
	ret.indices = RetrieveIndices(pMesh);

	ret.center = (min + max) * 0.5f;
	ret.extents = max - ret.center;

	return ret;
}

ObjectCreationData GenericLoader::LoadObjectFile(std::string path, int index)
{
	ObjectCreationData ret{};

	// this takes the filename out of the full path by indexing the \'s and the file extension
	std::string fileNameWithExtension = path.substr(path.find_last_of("/\\") + 1);
	ret.name = fileNameWithExtension.substr(0, fileNameWithExtension.find_last_of('.'));

	const aiScene* scene = aiImportFile(path.c_str(), aiProcessPreset_TargetRealtime_Fast);

	if (scene == nullptr) // check if the file could be read
		throw std::runtime_error("Failed to find or read file at " + path);

	ret.amountOfMeshes = scene->mNumMeshes;
	for (int i = 0; i < scene->mNumMeshes; i++) // convert the assimp resources into the engines resources
	{
		aiMesh* pMesh = scene->mMeshes[i];
		aiMaterial* pMaterial = scene->mMaterials[pMesh->mMaterialIndex];
		ret.meshes.push_back(RetrieveMeshData(pMesh, index));
	}
	return ret;
}