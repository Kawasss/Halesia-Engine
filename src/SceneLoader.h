#pragma once
#include <iostream>
#include <string>
#include <fstream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <execution>
#include "glm.h"
#include "Vertex.h"

constexpr int textureCoordinateOffset = 12;
constexpr int normalOffset = 20;

struct MaterialCreationData // dont know how smart it is to copy around possible megabytes of data, maybe make the stream read to the vector.data()
{
	std::string name;
	bool albedoIsDefault;
	std::vector<char> albedoData;
	bool normalIsDefault;
	std::vector<char> normalData;
	bool metallicIsDefault;
	std::vector<char> metallicData;
	bool roughnessIsDefault;
	std::vector<char> roughnessData;
	bool ambientOcclusionIsDefault;
	std::vector<char> ambientOcclusionData;
	bool heightIsDefault;
	std::vector<char> heightData;
};

struct MeshCreationData
{
	std::string name;
	MaterialCreationData material;

	bool hasBones;
	bool hasMaterial;
	
	glm::vec3 center = glm::vec3(0), extents = glm::vec3(0);

	int amountOfVertices;
	std::vector<Vertex> vertices;
	//int amountOfIndices;
	std::vector<uint16_t> indices;
};

struct ObjectCreationData
{
	std::string name = "";
	glm::vec3 position = glm::vec3(0);
	glm::vec3 rotation = glm::vec3(0);
	glm::vec3 scale = glm::vec3(1);

	int amountOfMeshes = 0;
	std::vector<MeshCreationData> meshes;
};

class SceneLoader
{
public:
	SceneLoader(std::string sceneLocation);

	void LoadScene();

	// camera related info
	glm::vec3 cameraPos;
	float cameraPitch, cameraYaw;

	// light related info
	// int amountOfLights; not impelemented yet
	glm::vec3 lightPos;

	// model related info
	int amountOfObjects;
	std::vector<ObjectCreationData> objects;

private:
	// file specific info
	std::string location;
	std::string header;
	std::ifstream stream;

	template<typename T> T GetType(char* bytes)
	{
		T f = 0;
		memcpy(&f, bytes, sizeof(T));
		return f;
	}

	glm::vec3 GetVec3(char* bytes);
	glm::vec2 GetVec2(char* bytes);

	std::string RetrieveName();
	glm::vec3 RetrieveTransformData();
	void RetrieveTexture(std::vector<char>& vectorToWriteTo, bool& isDefault);
	void RetrieveOneMaterial(MaterialCreationData& creationData);
	Vertex RetrieveOneVertex();
	void RetrieveOneMesh(MeshCreationData& creationData);
	void RetrieveOneObject(int index);
	void RetrieveAllObjects();
	void RetrieveObjectVariables();
	void RetrieveCameraVariables();
	void RetrieveLightVariables();
	void RetrieveHeader();

	void OpenInputFile(std::string path);
};

namespace GenericLoader
{
	ObjectCreationData LoadObjectFile(std::string path);
}