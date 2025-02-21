#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <filesystem>

#include "io/SceneLoader.h"

#include "core/Console.h"

#pragma pack(push, 1)
struct CompressionNode
{
	NodeType type = NODE_TYPE_NONE;
	NodeSize nodeSize = 0;
	uint64_t fileSize = 0; // size of the rest of the file (uncompressed)
	uint32_t mode = 0;
};
#pragma pack(pop)

SceneLoader::SceneLoader(std::string sceneLocation) : reader(BinaryReader(sceneLocation)), location(sceneLocation) {}

void SceneLoader::LoadScene() 
{
	std::filesystem::path path = location;
	if (path.extension() == ".hsf")
		LoadHSFFile();
	else
		LoadAssimpFile();
}

void SceneLoader::LoadHSFFile()
{
	reader.DecompressFile();

	NodeType type = NODE_TYPE_NONE;
	NodeSize size = 0;

	while (!reader.IsAtEndOfFile())
	{
		GetNodeHeader(type, size);
		RetrieveType(type, size);
	}
}

void SceneLoader::RetrieveType(NodeType type, NodeSize size)
{
	if (reader.IsAtEndOfFile())
		return;

	NodeType childType = NODE_TYPE_NONE;
	NodeSize childSize = 0;
	switch (type)
	{
	case NODE_TYPE_OBJECT:
		objects.push_back({});
		currentObject = objects.begin() + objects.size() - 1;
		reader >> currentObject->state;
		GetNodeHeader(childType, childSize);
		RetrieveType(childType, childSize); // name
		GetNodeHeader(childType, childSize);
		RetrieveType(childType, childSize); // transform
		GetNodeHeader(childType, childSize);
		RetrieveType(childType, childSize); // rigid body
		GetNodeHeader(childType, childSize);
		RetrieveType(childType, childSize); // mesh

		currentObject->hasMesh = !currentObject->mesh.vertices.empty();
		break;
	case NODE_TYPE_MESH:
	{
		FileMesh mesh;
		reader >> mesh;
		currentObject->hasMesh = true;
		currentObject->mesh.TransferFrom(mesh);
		break;
	}
	case NODE_TYPE_RIGIDBODY:
	{
		FileRigidBody rigid;
		reader >> rigid;
		currentObject->hitBox.TransferFrom(rigid);
		break;
	}
	case NODE_TYPE_NAME:
		reader >> currentObject->name;
		break;
	case NODE_TYPE_TRANSFORM:
		reader >> currentObject->position >> currentObject->rotation >> currentObject->scale;
		break;
	case NODE_TYPE_MATERIAL:
		materials.push_back({});
		reader >> materials.back();
		break;
	default:
		Console::WriteLine("Encountered an unusable node type " + std::to_string((int)type));
		uint8_t* junk = new uint8_t[size];
		reader.Read((char*)junk, size);
		delete[] junk;
		break;
	}
}

void SceneLoader::GetNodeHeader(NodeType& type, NodeSize& size)
{
	reader >> type >> size;
}

inline glm::vec3 Min(const glm::vec3& v1, const glm::vec3& v2)
{
	float x = v1.x < v2.x ? v1.x : v2.x;
	float y = v1.y < v2.y ? v1.y : v2.y;
	float z = v1.z < v2.z ? v1.z : v2.z;

	return { x, y, z };
}

inline glm::vec3 Max(const glm::vec3& v1, const glm::vec3& v2)
{
	float x = v1.x > v2.x ? v1.x : v2.x;
	float y = v1.y > v2.y ? v1.y : v2.y;
	float z = v1.z > v2.z ? v1.z : v2.z;

	return { x, y, z };
}

inline glm::vec3 ConvertAiVec3(const aiVector3D& vec)
{
	return { vec.x, vec.y, vec.z };
}

inline std::vector<Vertex> RetrieveVertices(aiMesh* pMesh, glm::vec3& min, glm::vec3& max)
{
	std::vector<Vertex> ret;
	for (int i = 0; i < pMesh->mNumVertices; i++)
	{
		Vertex vertex{};

		vertex.position = ConvertAiVec3(pMesh->mVertices[i]);

		max = Max(vertex.position, max);
		min = Min(vertex.position, min);

		if (pMesh->HasNormals())
			vertex.normal = ConvertAiVec3(pMesh->mNormals[i]);

		if (pMesh->mTextureCoords[0])
			vertex.textureCoordinates = ConvertAiVec3(pMesh->mTextureCoords[0][i]);
		ret.push_back(vertex);
	}
	return ret;
}

inline std::vector<uint16_t> RetrieveIndices(aiMesh* pMesh)
{
	std::vector<uint16_t> ret;
	for (int i = 0; i < pMesh->mNumFaces; i++)
		for (int j = 0; j < pMesh->mFaces[i].mNumIndices; j++)
			ret.push_back(pMesh->mFaces[i].mIndices[j]);
	return ret;
}

inline glm::mat4 GetMat4(const aiMatrix4x4& from)
{
	glm::mat4 to{};
	//the a,b,c,d in assimp is the row ; the 1,2,3,4 is the column
	to[0][0] = from.a1; to[1][0] = from.a2; to[2][0] = from.a3; to[3][0] = from.a4;
	to[0][1] = from.b1; to[1][1] = from.b2; to[2][1] = from.b3; to[3][1] = from.b4;
	to[0][2] = from.c1; to[1][2] = from.c2; to[2][2] = from.c3; to[3][2] = from.c4;
	to[0][3] = from.d1; to[1][3] = from.d2; to[2][3] = from.d3; to[3][3] = from.d4;
	return to;
}

inline aiMatrix4x4 GetMatrix4x4(const glm::mat4& from)
{
	aiMatrix4x4 to{};
	//the a,b,c,d in assimp is the row ; the 1,2,3,4 is the column
	to.a1 = from[0][0]; to.a2 = from[1][0]; to.a3 = from[2][0]; to.a4 = from[3][0];
	to.b1 = from[0][1]; to.b2 = from[1][1]; to.b3 = from[2][1]; to.b4 = from[3][1];
	to.c1 = from[0][2]; to.c2 = from[1][2]; to.c3 = from[2][2]; to.c4 = from[3][2];
	to.d1 = from[0][3]; to.d2 = from[1][3]; to.d3 = from[2][3]; to.d4 = from[3][3];
	return to;
}

inline void SetVertexBones(Vertex& vertex, int ID, float weight)
{
	for (int i = 0; i < MAX_BONES_PER_VERTEX; i++)
	{
		if (vertex.boneIndices[i] >= 0)
			continue;

		vertex.boneWeights[i] = weight;
		vertex.boneIndices[i] = ID;
		break; // break?
	}
}

void SceneLoader::RetrieveBoneData(MeshCreationData& creationData, const aiMesh* pMesh)
{
	if (!pMesh->HasBones())
		return;

	for (int i = 0; i < pMesh->mNumBones; i++)
	{
		int ID = -1;
		std::string name = pMesh->mBones[i]->mName.C_Str();
		if (boneInfoMap.find(name) == boneInfoMap.end())
		{
			BoneInfo info{};
			info.index = i;
			info.offset = GetMat4(pMesh->mBones[i]->mOffsetMatrix);
			boneInfoMap[name] = info;
			ID = i;
		}
		else ID = boneInfoMap[name].index;

		if (ID == -1) throw std::runtime_error("Failed to retrieve bone data");
		aiVertexWeight* weights = pMesh->mBones[i]->mWeights;
		
		for (int j = 0; j < pMesh->mBones[i]->mNumWeights; j++)
			if (weights[j].mWeight != 0.0f)
				SetVertexBones(creationData.vertices[weights[j].mVertexId], ID, weights[j].mWeight);
	}
}

MeshCreationData SceneLoader::RetrieveMeshData(aiMesh* pMesh)
{
	MeshCreationData ret{};

	glm::vec3 min = glm::vec3(0), max = glm::vec3(0);
	ret.amountOfVertices = pMesh->mNumVertices;
	ret.faceCount = pMesh->mNumFaces;
	ret.vertices = RetrieveVertices(pMesh, min, max);
	ret.indices = RetrieveIndices(pMesh);
	RetrieveBoneData(ret, pMesh);

	ret.center = (min + max) * 0.5f;
	ret.extents = max - ret.center;

	ret.name = pMesh->mName.C_Str();
	if (ret.name == "") ret.name = "NO_NAME";

	return ret;
}

inline MeshCreationData GetMeshFromAssimp(aiMesh* pMesh)
{
	MeshCreationData ret{};

	glm::vec3 min = glm::vec3(0), max = glm::vec3(0);
	ret.amountOfVertices = pMesh->mNumVertices;
	ret.faceCount = pMesh->mNumFaces;
	ret.vertices = RetrieveVertices(pMesh, min, max);
	ret.indices = RetrieveIndices(pMesh);

	ret.center = (min + max) * 0.5f;
	ret.extents = max - ret.center;

	ret.name = pMesh->mName.C_Str();
	if (ret.name.empty())
		ret.name = "NO_NAME";

	return ret;
}

inline glm::vec3 GetExtentsFromMesh(aiMesh* pMesh)
{
	glm::vec3 min = glm::vec3(0), max = glm::vec3(0);
	for (int i = 0; i < pMesh->mNumVertices; i++)
	{
		max.x = pMesh->mVertices[i].x > max.x ? pMesh->mVertices[i].x : max.x;
		max.y = pMesh->mVertices[i].y > max.y ? pMesh->mVertices[i].y : max.y;
		max.z = pMesh->mVertices[i].z > max.z ? pMesh->mVertices[i].z : max.z;

		min.x = pMesh->mVertices[i].x < min.x ? pMesh->mVertices[i].x : min.x;
		min.y = pMesh->mVertices[i].y < min.y ? pMesh->mVertices[i].y : min.y;
		min.z = pMesh->mVertices[i].z < min.z ? pMesh->mVertices[i].z : min.z;
	}
	glm::vec3 center = (min + max) * 0.5f;
	return max - center;
}

inline void GetTransform(const aiMatrix4x4& mat, glm::vec3& pos, glm::quat& rot, glm::vec3& scale)
{
	glm::mat4 trans;
	memcpy(&trans, &mat, sizeof(glm::mat4));
	trans = glm::transpose(trans);
	glm::quat orientation;
	glm::vec3 skew;
	glm::vec4 perspective;
	glm::decompose(trans, scale, orientation, pos, skew, perspective);
	rot = glm::degrees(glm::eulerAngles(orientation));
	pos /= 100;
	//scale /= 100;
}

void SceneLoader::LoadAssimpFile()
{
	const aiScene* scene = aiImportFile(location.c_str(), aiProcess_Triangulate | aiProcess_GenNormals);
	if (scene == nullptr) // check if the file could be read
		throw std::runtime_error("Failed to find or read file at " + location);

	objects.push_back(RetrieveObject(scene, scene->mRootNode, glm::mat4(1)));
	if (!scene->HasAnimations())
		return;
	for (int i = 0; i < scene->mNumAnimations; i++)
	{
		animations.push_back(Animation(scene->mAnimations[i], scene->mRootNode, boneInfoMap));
	}
}

ObjectCreationData SceneLoader::RetrieveObject(const aiScene* scene, const aiNode* node, glm::mat4 parentTrans)
{
	ObjectCreationData creationData;
	GetTransform(node->mTransformation, creationData.position, creationData.rotation, creationData.scale);

	for (int i = 0; i < node->mNumMeshes; i++)
	{
		aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
		creationData.hasMesh = node->mNumMeshes > 0;

		if (i != 0 || !creationData.hasMesh)
			continue;
		creationData.mesh = RetrieveMeshData(mesh);
	}
	
	if (node->mNumChildren > 0)
		creationData.children.reserve(node->mNumChildren);

	for (int i = 0; i < node->mNumChildren; i++)
		creationData.children.push_back(RetrieveObject(scene, node->mChildren[i], GetMat4(node->mTransformation)));
	
	return creationData;
}

ObjectCreationData GenericLoader::LoadObjectFile(std::string path) // kinda funky now, maybe make the funcion return multiple objects instead of one
{
	ObjectCreationData ret{};

	std::filesystem::path dir = path;
	ret.name = dir.stem().string();

	const aiScene* scene = aiImportFile(path.c_str(), aiProcessPreset_TargetRealtime_Fast);
	
	if (scene == nullptr) // check if the file could be read
		throw std::runtime_error("Failed to find or read file at " + path);

	ret.hasMesh = scene->mNumMeshes > 0;
	if (!ret.hasMesh)
		return ret;

	aiMesh* pMesh = scene->mMeshes[0];
	aiMaterial* pMaterial = scene->mMaterials[pMesh->mMaterialIndex];
	ret.mesh = GetMeshFromAssimp(pMesh);

	return ret;
}

glm::vec3 GenericLoader::LoadHitBox(std::string path)
{
	const aiScene* scene = aiImportFile(path.c_str(), aiProcessPreset_TargetRealtime_Fast);

	if (scene == nullptr)
		throw std::runtime_error("Cannot load a file: the file is invalid");
	if (scene->mNumMeshes <= 0)
		throw std::runtime_error("Cannot get the hitbox from a file: there are no meshes to get data from");
	return GetExtentsFromMesh(scene->mMeshes[0]);
}