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
#include "physics/Shapes.h"
#include "physics/RigidBody.h"

constexpr int textureCoordinateOffset = 12;
constexpr int normalOffset = 20;

enum ObjectFlags
{
	OBJECT_FLAG_HITBOX = 1 << 0,
	OBJECT_FLAG_RIGID_STATIC = 1 << 1,
	OBJECT_FLAG_RIGID_DYNAMIC = 1 << 2,
	OBJECT_FLAG_SHAPE_SPHERE = 1 << 3,
	OBJECT_FLAG_SHAPE_BOX = 1 << 4,
	OBJECT_FLAG_SHAPE_CAPSULE = 1 << 5
};

struct MaterialCreationData // dont know how smart it is to copy around possible megabytes of data, maybe make the stream read to the vector.data()
{
	std::string name;

	bool albedoIsDefault;
	bool normalIsDefault;
	bool metallicIsDefault;
	bool roughnessIsDefault;
	bool ambientOcclusionIsDefault;
	bool heightIsDefault;

	std::vector<char> albedoData;
	std::vector<char> normalData;
	std::vector<char> metallicData;
	std::vector<char> roughnessData;
	std::vector<char> ambientOcclusionData;
	std::vector<char> heightData;
};

struct MeshCreationData
{
	std::string name;
	MaterialCreationData material;

	bool hasBones;
	bool hasMaterial;
	
	glm::vec3 center = glm::vec3(0), extents = glm::vec3(0);

	int faceCount;
	int amountOfVertices;
	std::vector<Vertex> vertices;
	std::vector<uint16_t> indices;
};

struct HitboxCreationData
{
	glm::vec3 position = glm::vec3(0);
	glm::vec3 rotation = glm::vec3(0);
	glm::vec3 extents = glm::vec3(0);
	ShapeType type = SHAPE_TYPE_NONE;
	RigidBodyType rigidType = RIGID_BODY_NONE;
};

struct ObjectCreationData
{
	std::string name = "NO_NAME";
	glm::vec3 position = glm::vec3(0);
	glm::vec3 rotation = glm::vec3(0);
	glm::vec3 scale = glm::vec3(1);
	HitboxCreationData hitBox;
	
	int amountOfMeshes = 0;
	std::vector<MeshCreationData> meshes;
};

class SceneLoader
{
public:
	SceneLoader(std::string sceneLocation);

	void LoadScene();
	void LoadFBXScene();

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
	uint8_t RetrieveFlagsFromName(std::string string, std::string& name);

	void OpenInputFile(std::string path);
};

inline namespace GenericLoader
{
	glm::vec3 LoadHitBox(std::string path);
	ObjectCreationData LoadObjectFile(std::string path);
	MaterialCreationData LoadCPBRMaterial(std::string path);
}