#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <filesystem>

#include <hsl/StackMap.h>

#include <Windows.h>
#include <compressapi.h>

#include "io/SceneLoader.h"

#include "core/Console.h"
#include "core/UniquePointer.h"

#pragma pack(push, 1)
struct CompressionNode
{
	NodeType type = NODE_TYPE_NONE;
	NodeSize nodeSize = 0;
	uint64_t fileSize = 0; // size of the rest of the file (uncompressed)
	uint32_t mode = 0;
};
#pragma pack(pop)

template<typename T> inline T GetTypeFromStream(std::ifstream& stream)
{
	T f;
	stream.read((char*)&f, sizeof(T));
	return f;
}

BinaryReader::BinaryReader(std::string source) : input(std::ifstream(source, std::ios::in | std::ios::binary)) {}

void BinaryReader::DecompressFile()
{
	CompressionNode compression{};

	input.read(reinterpret_cast<char*>(&compression), sizeof(compression));

	if (compression.type != NODE_TYPE_COMPRESSION)
		throw std::runtime_error("Cannot determine the needed compression information");

	size_t compressedSize = static_cast<size_t>(input.seekg(0, std::ios::end).tellg()) - sizeof(CompressionNode);

	stream.resize(compression.fileSize);
	UniquePointer<char> compressed = new char[compressedSize];

	ReadCompressedData(compressed.Get(), compressedSize);

	input.close();

	size_t uncompressedSize = DecompressData(compressed.Get(), stream.data(), compression.mode, compressedSize, compression.fileSize);

	if (compression.fileSize != uncompressedSize)
		throw std::runtime_error("Could not properly decompress the file: there is a mismatch between the reported sizes (" + std::to_string(compression.fileSize) + " != " + std::to_string(uncompressedSize) + ")");
}

void BinaryReader::ReadCompressedData(char* src, size_t size)
{
	input.seekg(sizeof(CompressionNode), std::ios::beg);
	input.read(src, size);
}

size_t BinaryReader::DecompressData(char* src, char* dst, uint32_t mode, size_t size, size_t expectedSize)
{
	size_t uncompressedSize = 0;
	DECOMPRESSOR_HANDLE decompressor;

	if (!CreateDecompressor(mode, NULL, &decompressor))
		throw std::runtime_error("Cannot create a compressor"); // XPRESS is fast but not the best compression, XPRESS with huffman has better compression but is slower, MSZIP uses more resources and LZMS is slow. its Using xpress right now since its the fastest
	if (!Decompress(decompressor, src, size, dst, expectedSize, &uncompressedSize))
		throw std::runtime_error("Cannot decompress");
	if (!CloseDecompressor(decompressor))
		throw std::runtime_error("Cannot close a compressor");

	return uncompressedSize;
}

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
		break;
	case NODE_TYPE_MESH:
		currentObject->hasMesh = true;
		currentMesh = &currentObject->mesh;
		reader >> currentMesh->materialIndex;
		GetNodeHeader(childType, childSize);
		RetrieveType(childType, childSize); // vertices
		GetNodeHeader(childType, childSize);
		RetrieveType(childType, childSize); // indices
		currentMesh->faceCount = currentMesh->indices.size() / 3;
		break;
	case NODE_TYPE_VERTICES:
		currentMesh->vertices.resize(size / sizeof(Vertex));
		reader >> currentMesh->vertices;
		break;
	case NODE_TYPE_INDICES:
		currentMesh->indices.resize(size / sizeof(uint16_t));
		reader >> currentMesh->indices;
		break;
	case NODE_TYPE_RIGIDBODY:
		reader >> currentObject->hitBox.rigidType >> currentObject->hitBox.shapeType >> currentObject->hitBox.extents;
		break;
	case NODE_TYPE_NAME:
		reader >> currentObject->name;
		break;
	case NODE_TYPE_TRANSFORM:
		reader >> currentObject->position >> currentObject->rotation >> currentObject->scale;
		break;
	case NODE_TYPE_MATERIAL:
		materials.push_back({});
		currentMat = materials.begin() + materials.size() - 1;
		reader >> currentMat->isLight;
		GetNodeHeader(childType, childSize); // all textures
		RetrieveType(childType, childSize);
		GetNodeHeader(childType, childSize);
		RetrieveType(childType, childSize);
		GetNodeHeader(childType, childSize);
		RetrieveType(childType, childSize);
		GetNodeHeader(childType, childSize);
		RetrieveType(childType, childSize);
		GetNodeHeader(childType, childSize);
		RetrieveType(childType, childSize);
		break;
	case NODE_TYPE_ALBEDO:
		currentMat->albedoData.resize(size - sizeof(uint32_t) * 2);
		reader >> currentMat->aWidth >> currentMat->aHeight;
		reader >> currentMat->albedoData;
		break;
	case NODE_TYPE_NORMAL:
		currentMat->normalData.resize(size - sizeof(uint32_t) * 2);
		reader >> currentMat->nWidth >> currentMat->nHeight;
		reader >> currentMat->normalData;
		break;
	case NODE_TYPE_ROUGHNESS:
		currentMat->roughnessData.resize(size - sizeof(uint32_t) * 2);
		reader >> currentMat->rWidth >> currentMat->rHeight;
		reader >> currentMat->roughnessData;
		break;
	case NODE_TYPE_METALLIC:
		currentMat->metallicData.resize(size - sizeof(uint32_t) * 2);
		reader >> currentMat->mWidth >> currentMat->mHeight;
		reader >> currentMat->metallicData;
		break;
	case NODE_TYPE_AMBIENT_OCCLUSION:
		currentMat->ambientOcclusionData.resize(size - sizeof(uint32_t) * 2);
		reader >> currentMat->aoWidth >> currentMat->aoHeight;
		reader >> currentMat->ambientOcclusionData;
		break;
	default: 
		Console::WriteLine("Encountered an unusable node type");
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

uint8_t SceneLoader::RetrieveFlagsFromName(std::string string, std::string& name)
{
	static hsl::StackMap<std::string, ObjectFlags, 6> stringToFlag =
	{
		{ "rigid_static", OBJECT_FLAG_RIGID_STATIC }, { "rigid_dynamic", OBJECT_FLAG_RIGID_DYNAMIC }, { "hitbox",        OBJECT_FLAG_HITBOX        },
		{ "shape_sphere", OBJECT_FLAG_SHAPE_SPHERE }, { "shape_box",     OBJECT_FLAG_SHAPE_BOX     }, { "shape_capsule", OBJECT_FLAG_SHAPE_CAPSULE }
	};

	uint8_t ret = 0;
	name = "NO_NAME";
	std::string lexingString;
	for (int i = 0; i < string.size(); i++)
	{
		if (string[i] != '@' && i < string.size() - 1)
		{
			lexingString += string[i];
			continue;
		}
		if (i == string.size() - 1)
			lexingString += string[i];
		
		if (name == "NO_NAME")
			name = lexingString.empty() ? "NO_NAME" : lexingString;
		else if (stringToFlag.Contains(lexingString))
			ret |= stringToFlag[lexingString];
		else
			Console::WriteLine("Unrecognized object flag found: " + lexingString, Console::Severity::Warning);
		lexingString.clear();
	}
	return ret;
}

inline std::vector<Vertex> RetrieveVertices(aiMesh* pMesh, glm::vec3& min, glm::vec3& max)
{
	std::vector<Vertex> ret;
	for (int i = 0; i < pMesh->mNumVertices; i++)
	{
		Vertex vertex{};

		vertex.position = glm::vec3(pMesh->mVertices[i].x, pMesh->mVertices[i].y, pMesh->mVertices[i].z);

		max.x = vertex.position.x > max.x ? vertex.position.x : max.x;
		max.y = vertex.position.y > max.y ? vertex.position.y : max.y;
		max.z = vertex.position.z > max.z ? vertex.position.z : max.z;

		min.x = vertex.position.x < min.x ? vertex.position.x : min.x;
		min.y = vertex.position.y < min.y ? vertex.position.y : min.y;
		min.z = vertex.position.z < min.z ? vertex.position.z : min.z;

		if (pMesh->HasNormals())
			vertex.normal = glm::vec3(pMesh->mNormals[i].x, pMesh->mNormals[i].y, pMesh->mNormals[i].z);

		if (pMesh->mTextureCoords[0])
			vertex.textureCoordinates = glm::vec2(pMesh->mTextureCoords[0][i].x, pMesh->mTextureCoords[0][i].y);
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

inline void GetTransform(const aiMatrix4x4& mat, glm::vec3& pos, glm::vec3& rot, glm::vec3& scale)
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
	scale /= 100;
}

inline bool HasStaticRigidFlag(ObjectOptions flag)
{
	return flag & OBJECT_FLAG_RIGID_STATIC;
}

inline bool HasHitBoxFlag(ObjectOptions flag)
{
	return flag & OBJECT_FLAG_HITBOX;
}

inline Shape::Type GetShapeType(ObjectOptions flag)
{
	if (flag & OBJECT_FLAG_SHAPE_BOX)     return Shape::Type::Box;
	if (flag & OBJECT_FLAG_SHAPE_CAPSULE) return Shape::Type::Capsule;
	if (flag & OBJECT_FLAG_SHAPE_SPHERE)  return Shape::Type::Sphere;
	return Shape::Type::Box;
}

void SceneLoader::LoadAssimpFile()
{
	const aiScene* scene = aiImportFile(location.c_str(), aiProcess_Triangulate | aiProcess_GenNormals);
	if (scene == nullptr) // check if the file could be read
		throw std::runtime_error("Failed to find or read file at " + location);

	RetrieveObject(scene, scene->mRootNode, glm::mat4(1));
	if (!scene->HasAnimations())
		return;
	for (int i = 0; i < scene->mNumAnimations; i++)
	{
		animations.push_back(Animation(scene->mAnimations[i], scene->mRootNode, boneInfoMap));
	}
}

void SceneLoader::RetrieveObject(const aiScene* scene, const aiNode* node, glm::mat4 parentTrans)
{
	ObjectCreationData creationData;
	GetTransform(GetMatrix4x4(parentTrans) * node->mTransformation, creationData.position, creationData.rotation, creationData.scale);

	for (int i = 0; i < node->mNumMeshes; i++)
	{
		aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
		uint8_t flags = RetrieveFlagsFromName(mesh->mName.C_Str(), creationData.name);
		if (HasHitBoxFlag(flags))
		{
			RigidBody::Type rigidType = HasStaticRigidFlag(flags) ? RigidBody::Type::Static : RigidBody::Type::Dynamic;
			creationData.hitBox = { GetExtentsFromMesh(scene->mMeshes[node->mMeshes[0]]), GetShapeType(flags), rigidType };
		}
		else if (i == 0)
		{
			if (!(creationData.hasMesh = node->mNumMeshes > 0))
				continue;
			creationData.mesh = RetrieveMeshData(scene->mMeshes[node->mMeshes[i]]);
		}
	}
	for (int i = 0; i < node->mNumChildren; i++)
		RetrieveObject(scene, node->mChildren[i], parentTrans * GetMat4(node->mTransformation));
	
	objects.push_back(creationData);
}

ObjectCreationData GenericLoader::LoadObjectFile(std::string path) // kinda funky now, maybe make the funcion return multiple objects instead of one
{
	ObjectCreationData ret{};

	// this takes the filename out of the full path by indexing the \'s and the file extension
	std::string fileNameWithExtension = path.substr(path.find_last_of("/\\") + 1);
	ret.name = fileNameWithExtension.substr(0, fileNameWithExtension.find_last_of('.'));

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

inline void RetrieveTexture(std::vector<char>& vectorToWriteTo, std::ifstream& stream)
{
	int size = GetTypeFromStream<int>(stream);
	vectorToWriteTo.resize(size);
	stream.read(vectorToWriteTo.data(), size);
}

MaterialCreationData GenericLoader::LoadCPBRMaterial(std::string path)
{
	std::ifstream stream;
	stream.open(path, std::ios::in | std::ios::binary);
	if (!stream)
		throw std::runtime_error("failed to open file \"" + path + "\" since it can't be found");

	MaterialCreationData creationData{};

	RetrieveTexture(creationData.albedoData, stream);
	RetrieveTexture(creationData.normalData, stream);
	RetrieveTexture(creationData.metallicData, stream);
	RetrieveTexture(creationData.roughnessData, stream);
	RetrieveTexture(creationData.ambientOcclusionData, stream);
	RetrieveTexture(creationData.heightData, stream);

	return creationData;
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