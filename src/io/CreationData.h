#pragma once
#include <string>
#include <vector>

#include "../glm.h"

#include "../renderer/Vertex.h"

#include "../physics/Shapes.h"
#include "../physics/RigidBody.h"

struct MaterialCreationData // dont know how smart it is to copy around possible megabytes of data, maybe make the stream read to the vector.data()
{
	std::string name;

	uint32_t aWidth, aHeight, // albedo
		nWidth, nHeight,      // normal
		mWidth, mHeight,      // metallic
		rWidth, rHeight,      // roughness
		aoWidth, aoHeight;    // ambient occlusion

	bool isLight;

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
	uint32_t materialIndex;

	bool hasBones = false;
	bool hasMaterial = false;

	glm::vec3 center = glm::vec3(0);
	glm::vec3 extents = glm::vec3(0);

	int faceCount = 0;
	int amountOfVertices = 0;

	std::vector<Vertex>   vertices;
	std::vector<uint16_t> indices;
};

struct RigidCreationData
{
	glm::vec3 extents = glm::vec3(0);

	Shape::Type     shapeType = Shape::Type::None;
	RigidBody::Type rigidType = RigidBody::Type::None;
};

struct ObjectCreationData
{
	std::string name = "NO_NAME";

	glm::vec3 position = glm::vec3(0);
	glm::vec3 rotation = glm::vec3(0);
	glm::vec3 scale = glm::vec3(1);

	RigidCreationData hitBox;
	uint8_t state = 0;

	bool hasMesh = false;
	MeshCreationData mesh;
};