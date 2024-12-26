#pragma once
#include <string>
#include <vector>

#include "../glm.h"

#include "../renderer/Vertex.h"

#include "../physics/Shapes.h"
#include "../physics/RigidBody.h"

#include "FileMaterial.h"
#include "FileMesh.h"

using MaterialCreationData = FileMaterial;
using ImageCreationData    = FileImage;
//using MeshCreationData     = FileMesh;

struct MeshCreationData
{
	void TransferFrom(FileMesh& mesh) // will inherit the vertices and indices of the given FileMesh
	{
		vertices = std::move(mesh.vertices.data);
		indices  = std::move(mesh.indices.data);
	}

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
	glm::quat rotation = glm::quat();
	glm::vec3 scale    = glm::vec3(1);

	RigidCreationData hitBox;
	uint8_t state = 0;

	bool hasMesh = false;
	MeshCreationData mesh;
};