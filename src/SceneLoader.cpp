#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <unordered_map>

#include "io/SceneLoader.h"
#include "core/Console.h"

template<typename T> inline T GetType(char* bytes)
{
	T f = 0;
	memcpy(&f, bytes, sizeof(T));
	return f;
}

template<typename T> inline T GetTypeFromStream(std::ifstream& stream)
{
	char bytes[sizeof(T)] = { 0 };
	T f;
	memcpy(&f, bytes, sizeof(T));
	return f;
}

SceneLoader::SceneLoader(std::string sceneLocation)
{
	this->location = sceneLocation;
}

void SceneLoader::LoadScene()
{
	OpenInputFile(location);
	RetrieveHeader();
	RetrieveCameraVariables();
	RetrieveLightVariables();
	RetrieveObjectVariables();
	RetrieveAllObjects();
}

glm::vec3 SceneLoader::GetVec3(char* bytes)
{
	glm::vec3 ret;
	ret.x = GetType<float>(bytes);
	ret.y = GetType<float>(bytes + 4);
	ret.z = GetType<float>(bytes + 8);
	return ret;
}

glm::vec2 SceneLoader::GetVec2(char* bytes)
{
	glm::vec2 ret;
	ret.x = GetType<float>(bytes);
	ret.y = GetType<float>(bytes + 4);
	return ret;
}

std::string SceneLoader::RetrieveName()
{
	char nameBytes[11];
	stream.read(nameBytes, 10);
	nameBytes[10] = '\0';
	return (std::string)nameBytes;
}

glm::vec3 SceneLoader::RetrieveTransformData()
{
	char posBytes[12];
	stream.read(posBytes, 12);
	return GetVec3(posBytes);
}

void SceneLoader::RetrieveTexture(std::vector<char>& vectorToWriteTo, bool& isDefault)
{
	char isDefaultByte;
	stream.read(&isDefaultByte, 1);
	isDefault = GetType<int8_t>(&isDefaultByte);
	if (isDefault)
		return;

	char lengthBytes[4];
	stream.read(lengthBytes, 4);
	
	int textureLength = GetType<int>(lengthBytes);
	if (textureLength <= 0)
	{
		isDefault = true;
		return;
	}
	vectorToWriteTo = std::vector<char>(textureLength);
	stream.read(vectorToWriteTo.data(), textureLength);
}

void SceneLoader::RetrieveOneMaterial(MaterialCreationData& creationData)
{
	RetrieveTexture(creationData.albedoData, creationData.albedoIsDefault);
	RetrieveTexture(creationData.normalData, creationData.normalIsDefault);
	RetrieveTexture(creationData.metallicData, creationData.metallicIsDefault);
	RetrieveTexture(creationData.roughnessData, creationData.roughnessIsDefault);
	RetrieveTexture(creationData.ambientOcclusionData, creationData.ambientOcclusionIsDefault);
	RetrieveTexture(creationData.heightData, creationData.heightIsDefault);
}

Vertex SceneLoader::RetrieveOneVertex()
{
	Vertex ret;
	char vecBytes[32];
	stream.read(vecBytes, 32);
	ret.position = GetVec3(vecBytes);
	ret.textureCoordinates = GetVec2(vecBytes + 12);
	ret.normal = GetVec3(vecBytes + 20);
	return ret;
}

void SceneLoader::RetrieveOneMesh(MeshCreationData& creationData)
{
	creationData.name = RetrieveName();
	for (int i = 0; i < 3; i++) // the file contains the transform data of the meshes, but those are already processed into the objects transformation, so ignore these
		RetrieveTransformData();

	char containsBonesOrTextures;
	stream.read(&containsBonesOrTextures, 1);
	creationData.hasMaterial = GetType<int8_t>(&containsBonesOrTextures);
	stream.read(&containsBonesOrTextures, 1);
	creationData.hasBones = GetType<int8_t>(&containsBonesOrTextures);
	
	char amountOfVerticesBytes[4];
	stream.read(amountOfVerticesBytes, 4);
	creationData.amountOfVertices = GetType<int>(amountOfVerticesBytes) * 3;
	
	glm::vec3 max = glm::vec3(0), min = glm::vec3(0);
	for (int i = 0; i < creationData.amountOfVertices; i++)
	{
		Vertex vertex = RetrieveOneVertex();

		max.x = vertex.position.x > max.x ? vertex.position.x : max.x;
		max.y = vertex.position.y > max.y ? vertex.position.y : max.y;
		max.z = vertex.position.z > max.z ? vertex.position.z : max.z;

		min.x = vertex.position.x < min.x ? vertex.position.x : min.x;
		min.y = vertex.position.y < min.y ? vertex.position.y : min.y;
		min.z = vertex.position.z < min.z ? vertex.position.z : min.z;

		creationData.vertices.push_back(vertex); // could do resize like in RetrieveOneObject, but one mesh can have thousands of vertices so i dont know about the copying speed of that
		creationData.indices.push_back(i); // since CRS doesnt support indices (yet), this is filled like there are no indices
	}
	creationData.center = (min + max) * 0.5f;
	creationData.extents = max - creationData.center;
	
	if (creationData.hasMaterial)
		RetrieveOneMaterial(creationData.material);
}

void SceneLoader::RetrieveOneObject(int index)
{
	ObjectCreationData& creationData = objects[index];

	creationData.name = RetrieveName();
	creationData.position = RetrieveTransformData();
	creationData.scale = RetrieveTransformData();
	creationData.rotation = RetrieveTransformData();
	
	char amountOfMeshesBytes[4];
	stream.read(amountOfMeshesBytes, 4);
	
	creationData.amountOfMeshes = GetType<int>(amountOfMeshesBytes);
	creationData.meshes.resize(creationData.amountOfMeshes);
	
	for (int i = 0; i < creationData.amountOfMeshes; i++)
		RetrieveOneMesh(creationData.meshes[i]);
}

void SceneLoader::RetrieveAllObjects()
{
	for (int i = 0; i < amountOfObjects; i++)
		RetrieveOneObject(i);
}

void SceneLoader::RetrieveObjectVariables()
{
	char amountBytes[4];
	stream.read(amountBytes, 4);
	amountOfObjects = GetType<int>(amountBytes);

	objects.resize(amountOfObjects);
}

void SceneLoader::RetrieveCameraVariables()
{
	char cameraBytes[20];
	stream.read(cameraBytes, 20);
	cameraPos = GetVec3(cameraBytes);
	cameraPitch = GetType<float>(cameraBytes + 12);
	cameraYaw = GetType<float>(cameraBytes + 16);
}

void SceneLoader::RetrieveLightVariables()
{
	char lightBytes[12];
	stream.read(lightBytes, 12);
	lightPos = GetVec3(lightBytes);
}

void SceneLoader::RetrieveHeader()
{
	char lHeader[101];
	stream.read(lHeader, 100);
	lHeader[100] = '\0';
	this->header = std::string(lHeader);
}

void SceneLoader::OpenInputFile(std::string path)
{
	stream.open(path, std::ios::in | std::ios::binary);
	if (!stream)
		throw std::runtime_error("Failed to open the scene file at " + path + ", the path is either invalid or outdated / corrupt");
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

		if (pMesh->HasNormals() || ret.size() % 3 != 0)
			continue;

		Vertex& vert1 = ret[ret.size() - 3];
		Vertex& vert2 = ret[ret.size() - 2];
		Vertex& vert3 = ret.back();
		glm::vec3 norm = glm::cross(vert2.position - vert1.position, vert3.position - vert1.position);
		vert1.normal = norm;
		vert2.normal = norm;
		vert3.normal = norm;
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

inline void RetrieveBoneData(MeshCreationData& creationData, const aiMesh* pMesh)
{
	if (!pMesh->HasBones())
		return;

	for (int i = 0; i < pMesh->mNumBones; i++)
	{
		int ID = -1;
		std::string name = pMesh->mBones[i]->mName.C_Str();
		if (creationData.boneInfoMap.find(name) == creationData.boneInfoMap.end())
		{
			BoneInfo info{};
			info.index = i;
			info.offset = GetMat4(pMesh->mBones[i]->mOffsetMatrix);
			creationData.boneInfoMap[name] = info;
			ID = i;
		}
		else ID = creationData.boneInfoMap[name].index;

		if (ID == -1) throw std::runtime_error("Failed to retrieve bone data");
		aiVertexWeight* weights = pMesh->mBones[i]->mWeights;
		
		for (int j = 0; j < pMesh->mBones[i]->mNumWeights; j++)
			SetVertexBones(creationData.vertices[weights[j].mVertexId], ID, weights[j].mWeight);
	}
}

inline MeshCreationData RetrieveMeshData(aiMesh* pMesh)
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
	rot = glm::eulerAngles(orientation);
}

void operator |=(ObjectFlags& f1, ObjectFlags f2)
{
	f1 = f1 | f2;
}

ObjectFlags operator |(ObjectFlags f1, ObjectFlags f2)
{
	using type = std::underlying_type_t<ObjectFlags>;
	return (ObjectFlags)((type)f1 | (type)f2);
}

ObjectFlags operator &(ObjectFlags f1, ObjectFlags f2)
{
	using type = std::underlying_type_t<ObjectFlags>;
	return (ObjectFlags)((type)f1 & (type)f2);
}

inline bool FlagHasFlag(uint8_t flag1, ObjectFlags flag2)
{
	return (flag1 & flag2) == flag2;
}

inline bool HasStaticRigidFlag(uint8_t flag)
{
	return FlagHasFlag(flag, OBJECT_FLAG_RIGID_STATIC);
}

inline bool HasHitBoxFlag(uint8_t flag)
{
	return FlagHasFlag(flag, OBJECT_FLAG_HITBOX);
}

inline ShapeType GetShapeType(uint8_t flag)
{
	if (FlagHasFlag(flag, OBJECT_FLAG_SHAPE_BOX))
		return SHAPE_TYPE_BOX;
	if (FlagHasFlag(flag, OBJECT_FLAG_SHAPE_CAPSULE))
		return SHAPE_TYPE_CAPSULE;
	if (FlagHasFlag(flag, OBJECT_FLAG_SHAPE_SPHERE))
		return SHAPE_TYPE_SPHERE;
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
		glm::vec3 pos = glm::vec3(0), rot = glm::vec3(0), scale = glm::vec3(1);
		GetTransform(node->mTransformation, pos, rot, scale);
		float y = pos.y, z = pos.z;
		//pos.y = z;
		//pos.z = -y;
		pos /= 100;
		scale /= 100;
		rot = glm::degrees(rot);
		
		aiMesh* mesh = scene->mMeshes[0];
		uint8_t flags = RetrieveFlagsFromName(mesh->mName.C_Str(), creationData.name);

		creationData.position = pos;
		creationData.rotation = rot;
		if (HasHitBoxFlag(flags))
		{
			RigidBodyType rigidType = HasStaticRigidFlag(flags) ? RIGID_BODY_STATIC : RIGID_BODY_DYNAMIC;
			creationData.hitBox = { GetExtentsFromMesh(scene->mMeshes[node->mMeshes[0]]), GetShapeType(flags), rigidType };
		}
		else
		{
			creationData.scale = scale;
			for (int j = 0; j < node->mNumMeshes; j++)
				creationData.meshes.push_back(RetrieveMeshData(scene->mMeshes[node->mMeshes[j]]));
		}
		objects.push_back(creationData);
	}
	if (!scene->HasAnimations())
		return;
	for (int i = 0; i < scene->mNumAnimations; i++)
	{
		objects.back().meshes[0].animations.push_back(Animation(scene->mAnimations[i], scene->mRootNode, objects.back().meshes[0]));
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
		ret.meshes.push_back(RetrieveMeshData(pMesh));
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