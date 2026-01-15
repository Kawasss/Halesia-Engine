#pragma once
#include <string>
#include <vector>
#include <map>

#include "../glm.h"

#include "../renderer/Vertex.h"
#include "../renderer/Light.h"
#include "../renderer/Bone.h"

#include "FileMaterial.h"
#include "FileMesh.h"
#include "FileRigidBody.h"

import Physics.RigidBody;
import Physics.Shapes;

using MaterialCreationData = FileMaterial;
using ImageCreationData    = FileImage;
//using MeshCreationData     = FileMesh;

struct MeshCreationData
{
	void TransferFrom(FileMesh& mesh) // will inherit the vertices and indices of the given FileMesh
	{
		vertices = std::move(mesh.vertices.data);
		indices  = std::move(mesh.indices.data);

		materialIndex = mesh.materialIndex;
		faceCount = indices.size() / 3;
	}

	uint32_t materialIndex;

	bool hasBones = false;
	bool hasMaterial = false;
	bool cullBackFaces = true;

	glm::vec3 min = glm::vec3(0);
	glm::vec3 max = glm::vec3(0);

	int faceCount = 0;

	int flags = 0;

	std::vector<Vertex>   vertices;
	std::vector<uint32_t> indices;

	std::map<std::string, BoneInfo> bones;
};

struct RigidCreationData
{
	void TransferFrom(const FileRigidBody& file) // maybe move to source file
	{
		rigidType = static_cast<RigidBody::Type>(file.type);
		shapeType = static_cast<Shape::Type>(file.shape.type);

		extents = { file.shape.x, file.shape.y, file.shape.z };
	}

	glm::vec3 extents = glm::vec3(0);

	Shape::Type     shapeType = Shape::Type::None;
	RigidBody::Type rigidType = RigidBody::Type::None;
};

struct ObjectCreationData
{
	enum class Type
	{
		Base = 0,
		Mesh = 1,
		Rigid3D = 2,
		Light = 3,
		Script = 4,
	};

	std::string name = "NO_NAME";

	glm::vec3 position = glm::vec3(0);
	glm::quat rotation = glm::quat();
	glm::vec3 scale    = glm::vec3(1);

	RigidCreationData hitBox;
	uint8_t state = 0;

	bool hasMesh = false;
	MeshCreationData mesh;
	
	Light lightData;

	Type type = Type::Base;

	std::vector<ObjectCreationData> children;
	std::vector<char> unknownData; // unknown refers to data of which its purpose is unknown, this data can be used for i.e. superclasses that inherit from Objects (de)serialization pipeline
};