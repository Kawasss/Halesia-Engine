#include "SceneLoader.h"

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

void SceneLoader::RetrieveTexture(std::vector<char>& vectorToWriteTo)
{

	char isDefaultByte;
	stream.read(&isDefaultByte, 1);
	bool isDefault = GetType<int8_t>(&isDefaultByte);
	if (isDefault)
		return;

	char lengthBytes[4];
	stream.read(lengthBytes, 4);

	int textureLength = GetType<int>(lengthBytes + 1);
	vectorToWriteTo = std::vector<char>(textureLength);

	stream.read(vectorToWriteTo.data(), textureLength);
}

void SceneLoader::RetrieveOneMaterial(MaterialCreationData& creationData)
{
	creationData.name = RetrieveName();
	RetrieveTexture(creationData.albedoData);
	RetrieveTexture(creationData.normalData);
	RetrieveTexture(creationData.metallicData);
	RetrieveTexture(creationData.roughnessData);
	RetrieveTexture(creationData.ambientOcclusionData);
	RetrieveTexture(creationData.heightData);
}

Vertex SceneLoader::RetrieveOneVertex()
{
	Vertex ret;
	char vecBytes[32];
	stream.read(vecBytes, 32);
	ret.position = GetVec3(vecBytes);
	ret.textureCoordinates = GetVec2(vecBytes + 12);
	ret.normal = GetVec3(vecBytes + 20);

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

	for (int i = 0; i < creationData.amountOfVertices; i++)
	{
		creationData.vertices.push_back(RetrieveOneVertex()); // could do resize like in RetrieveOneObject, but one mesh can have thousands of vertices so i dont know about the copying speed of that
		creationData.indices.push_back(i); // since CRS doesnt support indices (yet), this is filled like there are no indices
	}
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
	this->header = std::string(lHeader);
}

void SceneLoader::OpenInputFile(std::string path)
{
	stream.open(path, std::ios::in | std::ios::binary);
	if (!stream)
		throw std::runtime_error("Failed to open the scene file at " + path + ", the path is either invalid or outdated / corrupt");
}