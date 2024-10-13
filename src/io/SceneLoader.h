#pragma once
#include <string>
#include <fstream>
#include <vector>
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
struct aiNode;
struct aiScene;

typedef uint32_t ObjectOptions;
enum ObjectFlags : ObjectOptions
{
	OBJECT_FLAG_HITBOX        = 1 << 0,
	OBJECT_FLAG_NO_RIGID      = 1 << 1,
	OBJECT_FLAG_RIGID_STATIC  = 1 << 2,
	OBJECT_FLAG_RIGID_DYNAMIC = 1 << 3,
	OBJECT_FLAG_SHAPE_SPHERE  = 1 << 4,
	OBJECT_FLAG_SHAPE_BOX     = 1 << 5,
	OBJECT_FLAG_SHAPE_CAPSULE = 1 << 6,
};

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

	bool hasBones    = false;
	bool hasMaterial = false;
	
	glm::vec3 center  = glm::vec3(0);
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
	glm::vec3 scale    = glm::vec3(1);

	RigidCreationData hitBox;
	uint8_t state = 0;
	
	bool hasMesh = false;
	MeshCreationData mesh;
};

class BinaryReader
{
public:
	BinaryReader() {}
	BinaryReader(std::string source);

	void DecompressFile();

	void Read(char* ptr, size_t size)
	{
		memcpy(ptr, stream.data() + pointer, size);
		pointer += size;
	}

	template<typename Type>
	BinaryReader& operator>>(Type& in)
	{
		memcpy((void*)&in, stream.data() + pointer, sizeof(Type));
		pointer += sizeof(Type);
		return *this;
	}

	template<typename Type>
	BinaryReader& operator>>(std::vector<Type>& vec) // this expects the vector to already be resized to the correct size
	{
		memcpy((void*)vec.data(), stream.data() + pointer, sizeof(Type) * vec.size());
		pointer += sizeof(Type) * vec.size();
		return *this;
	}

	BinaryReader& operator>>(std::string& str) // this expects the string in the file to be null terminated
	{
		str = "";
		while (stream[pointer] != '\0')
			str += stream[pointer++];
		pointer++;
		return *this;
	}

	bool IsAtEndOfFile() { return pointer >= stream.size() - 1; }

private:
	void ReadCompressedData(char* src, size_t size);
	size_t DecompressData(char* src, char* dst, uint32_t mode, size_t size, size_t expectedSize);

	size_t pointer = 0;
	std::vector<char> stream;
	std::ifstream input;
};

class SceneLoader
{
public:
	SceneLoader() {}
	SceneLoader(std::string sceneLocation);

	void LoadScene();

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

	void LoadHSFFile();
	void LoadAssimpFile();

	void RetrieveBoneData(MeshCreationData& creationData, const aiMesh* pMesh);
	MeshCreationData RetrieveMeshData(aiMesh* pMesh);

	uint8_t RetrieveFlagsFromName(std::string string, std::string& name); 

	void GetNodeHeader(NodeType& type, NodeSize& size);
	void RetrieveType(NodeType type, NodeSize size);
	void RetrieveObject(const aiScene* scene, const aiNode* node, glm::mat4 parentTrans);

	MeshCreationData* currentMesh = nullptr; // dont know how safe this is
	std::vector<ObjectCreationData>::iterator currentObject{};
	std::vector<MaterialCreationData>::iterator currentMat{};
};

namespace GenericLoader
{
	glm::vec3 LoadHitBox(std::string path);
	ObjectCreationData LoadObjectFile(std::string path);
	MaterialCreationData LoadCPBRMaterial(std::string path);
}