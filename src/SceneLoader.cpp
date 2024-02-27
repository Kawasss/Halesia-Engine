#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <unordered_map>

#include "io/SceneLoader.h"
#include "core/Console.h"

template<typename T> inline T GetTypeFromStream(std::ifstream& stream)
{
	T f;
	stream.read((char*)&f, sizeof(T));
	return f;
}

SceneLoader::SceneLoader(std::string sceneLocation) : reader(BinaryReader(sceneLocation)), location(sceneLocation) {}

void SceneLoader::LoadScene() 
{
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
		currentObject->meshes.push_back({});
		currentMesh = currentObject->meshes.begin() + currentObject->meshes.size() - 1;
		reader >> currentMesh->materialIndex;
		GetNodeHeader(childType, childSize);
		RetrieveType(childType, childSize); // vertices
		GetNodeHeader(childType, childSize);
		RetrieveType(childType, childSize); // indices
		currentMesh->faceCount = currentMesh->indices.size() / 3;
		if (currentMesh->vertices.empty())
			currentObject->meshes.erase(currentMesh);
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
	default: 
		std::cout << "unused node type " << NodeTypeToString(type) << " (" << type << ")\n";
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
	static std::unordered_map<std::string, ObjectFlags> stringToFlag =
	{
		{ "rigid_static", OBJECT_FLAG_RIGID_STATIC }, { "rigid_dynamic", OBJECT_FLAG_RIGID_DYNAMIC }, { "hitbox", OBJECT_FLAG_HITBOX },
		{ "shape_sphere", OBJECT_FLAG_SHAPE_SPHERE }, { "shape_box", OBJECT_FLAG_SHAPE_BOX }, { "shape_capsule", OBJECT_FLAG_SHAPE_CAPSULE }
	};

	uint8_t ret = 0;
	name = "NO_NAME";
	std::string lexingString = "";
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
			name = lexingString == "" ? "NO_NAME" : lexingString;
		else if (stringToFlag.count(lexingString) > 0)
			ret |= stringToFlag[lexingString];
		else
			Console::WriteLine("Unrecognized object flag found: " + lexingString, MESSAGE_SEVERITY_WARNING);
		lexingString = "";
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
	if (ret.name == "") ret.name = "NO_NAME";

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

inline void GetTransform(aiMatrix4x4 mat, glm::vec3& pos, glm::vec3& rot, glm::vec3& scale)
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

inline ShapeType GetShapeType(ObjectOptions flag)
{
	if (flag & OBJECT_FLAG_SHAPE_BOX)     return SHAPE_TYPE_BOX;
	if (flag & OBJECT_FLAG_SHAPE_CAPSULE) return SHAPE_TYPE_CAPSULE;
	if (flag & OBJECT_FLAG_SHAPE_SPHERE)  return SHAPE_TYPE_SPHERE;
	return SHAPE_TYPE_BOX;
}

void SceneLoader::LoadFBXScene()
{
	const aiScene* scene = aiImportFile(location.c_str(), aiProcess_Triangulate);
	if (scene == nullptr) // check if the file could be read
		throw std::runtime_error("Failed to find or read file at " + location);

	for (int i = 0; i < scene->mRootNode->mNumChildren; i++)
	{
		ObjectCreationData creationData;
		aiNode* node = scene->mRootNode->mChildren[i];
		GetTransform(node->mTransformation, creationData.position, creationData.rotation, creationData.scale);
		
		aiMesh* mesh = scene->mMeshes[0];
		uint8_t flags = RetrieveFlagsFromName(mesh->mName.C_Str(), creationData.name);
		if (HasHitBoxFlag(flags))
		{
			RigidBodyType rigidType = HasStaticRigidFlag(flags) ? RIGID_BODY_STATIC : RIGID_BODY_DYNAMIC;
			creationData.hitBox = { GetExtentsFromMesh(scene->mMeshes[node->mMeshes[0]]), GetShapeType(flags), rigidType };
		}
		else
		{
			for (int j = 0; j < node->mNumMeshes; j++)
				creationData.meshes.push_back(RetrieveMeshData(scene->mMeshes[node->mMeshes[j]]));
		}
		objects.push_back(creationData);
	}
	if (!scene->HasAnimations())
		return;
	for (int i = 0; i < scene->mNumAnimations; i++)
	{
		animations.push_back(Animation(scene->mAnimations[i], scene->mRootNode, boneInfoMap));
	}
}

ObjectCreationData GenericLoader::LoadObjectFile(std::string path)
{
	ObjectCreationData ret{};

	// this takes the filename out of the full path by indexing the \'s and the file extension
	std::string fileNameWithExtension = path.substr(path.find_last_of("/\\") + 1);
	ret.name = fileNameWithExtension.substr(0, fileNameWithExtension.find_last_of('.'));

	const aiScene* scene = aiImportFile(path.c_str(), aiProcessPreset_TargetRealtime_Fast);
	
	if (scene == nullptr) // check if the file could be read
		throw std::runtime_error("Failed to find or read file at " + path);

	ret.amountOfMeshes = scene->mNumMeshes;
	for (int i = 0; i < scene->mNumMeshes; i++) // convert the assimp resources into the engines resources
	{
		aiMesh* pMesh = scene->mMeshes[i];
		aiMaterial* pMaterial = scene->mMaterials[pMesh->mMaterialIndex];
		ret.meshes.push_back(GetMeshFromAssimp(pMesh));
	}
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