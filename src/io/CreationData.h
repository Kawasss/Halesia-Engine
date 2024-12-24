#pragma once
#include <string>
#include <vector>

#include "../glm.h"

#include "../renderer/Vertex.h"

#include "../physics/Shapes.h"
#include "../physics/RigidBody.h"

#include "FileMaterial.h"

using MaterialCreationData = FileMaterial;
using ImageCreationData    = FileImage;

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