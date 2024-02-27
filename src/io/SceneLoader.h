#pragma once
#include <iostream>
#include <string>
#include <fstream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <execution>
#include <map>
#include "glm.h"
#include "renderer/Vertex.h"
#include "renderer/Bone.h"
#include "renderer/AnimationManager.h"
#include "physics/Shapes.h"
#include "physics/RigidBody.h"

#include "FileFormat.h"

constexpr int textureCoordinateOffset = 12;
constexpr int normalOffset = 20;

struct aiMesh;

typedef uint32_t ObjectOptions;
enum ObjectFlags : ObjectOptions
{
	OBJECT_FLAG_HITBOX = 1 << 0,
	OBJECT_FLAG_NO_RIGID = 1 << 1,
	OBJECT_FLAG_RIGID_STATIC = 1 << 2,
	OBJECT_FLAG_RIGID_DYNAMIC = 1 << 3,
	OBJECT_FLAG_SHAPE_SPHERE = 1 << 4,
	OBJECT_FLAG_SHAPE_BOX = 1 << 5,
	OBJECT_FLAG_SHAPE_CAPSULE = 1 << 6,
};

struct MaterialCreationData // dont know how smart it is to copy around possible megabytes of data, maybe make the stream read to the vector.data()
{
	std::string name;

	bool isLight = false;
	bool albedoIsDefault = true;
	bool normalIsDefault = true;
	bool metallicIsDefault = true;
	bool roughnessIsDefault = true;
	bool ambientOcclusionIsDefault = true;
	bool heightIsDefault = true;

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
	
	glm::vec3 center = glm::vec3(0), extents = glm::vec3(0);

	int faceCount = 0;
	int amountOfVertices = 0;
	std::vector<Vertex> vertices;
	std::vector<uint16_t> indices;
};

struct RigidCreationData
{
	glm::vec3 extents = glm::vec3(0);
	ShapeType shapeType = SHAPE_TYPE_NONE;
	RigidBodyType rigidType = RIGID_BODY_NONE;
};

struct ObjectCreationData
{
	std::string name = "NO_NAME";
	glm::vec3 position = glm::vec3(0);
	glm::vec3 rotation = glm::vec3(0);
	glm::vec3 scale = glm::vec3(1);
	RigidCreationData hitBox;
	uint8_t state = 0;
	
	int amountOfMeshes = 0;
	std::vector<MeshCreationData> meshes;
};

class BinaryReader
{
public:
	BinaryReader(std::string source) : stream(std::ifstream(source, std::ios::in | std::ios::binary)) {}

	void Read(char* ptr, size_t size)
	{
		stream.read(ptr, size);
	}

	template<typename Type>
	BinaryReader& operator>>(Type& in)
	{
		stream.read((char*)&in, sizeof(in));
		return *this;
	}

	template<typename Type>
	BinaryReader& operator>>(std::vector<Type>& vec) // this expects the vector to already be resized to the correct size
	{
		stream.read((char*)vec.data(), sizeof(Type) * vec.size());
		return *this;
	}

	BinaryReader& operator>>(std::string& str) // this expects the string in the file to be null terminated
	{
		str = "";
		for (int i = 0; i == 0 || str.back() != '\0'; i++)
		{
			char ch;
			stream.read(&ch, sizeof(ch));
			str += ch;
		}
		str.pop_back();
		return *this;
	}

	bool IsAtEndOfFile() { return stream.eof(); }

private:
	std::ifstream stream;
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
	std::vector<MaterialCreationData> materials;

	// animations
	std::vector<Animation> animations;
	std::map<std::string, BoneInfo> boneInfoMap;

private:
	// file specific info
	std::string header;
	std::string location;
	BinaryReader reader;

	void RetrieveBoneData(MeshCreationData& creationData, const aiMesh* pMesh);
	MeshCreationData RetrieveMeshData(aiMesh* pMesh);

	uint8_t RetrieveFlagsFromName(std::string string, std::string& name); 

	void GetNodeHeader(NodeType& type, NodeSize& size);
	void RetrieveType(NodeType type, NodeSize size);

	std::vector<MeshCreationData>::iterator currentMesh;
	std::vector<ObjectCreationData>::iterator currentObject;
	std::vector<MaterialCreationData>::iterator currentMat;
};

inline namespace GenericLoader
{
	glm::vec3 LoadHitBox(std::string path);
	ObjectCreationData LoadObjectFile(std::string path);
	MaterialCreationData LoadCPBRMaterial(std::string path);
}