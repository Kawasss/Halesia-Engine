#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <filesystem>

#include "io/SceneLoader.h"
#include "io/DataArchiveFile.h"
#include "io/BinaryStream.h"

#include "core/Console.h"
#include "core/Object.h"

namespace fs = std::filesystem;

constexpr std::string_view CUSTOM_FILE_EXTENSION = ".dat";

#pragma pack(push, 1)
struct CompressionNode
{
	NodeType type = NODE_TYPE_NONE;
	NodeSize nodeSize = 0;
	uint64_t fileSize = 0; // size of the rest of the file (uncompressed)
	uint32_t mode = 0;
};
#pragma pack(pop)

SceneLoader::SceneLoader(std::string sceneLocation) : location(sceneLocation) {}

void SceneLoader::LoadScene() 
{
	fs::path path = location;
	if (path.extension() == CUSTOM_FILE_EXTENSION)
		LoadCustomFile();
	else
		LoadAssimpFile();
}

static std::vector<std::string> ReadNamedReferences(const BinarySpan& data) // maybe use string_view for small performance boost ??
{
	uint32_t count = 0;
	data >> count;

	std::vector<std::string> ret(count);

	for (uint32_t i = 0; i < count; i++)
	{
		uint32_t strLen = 0;
		data >> strLen;

		ret[i].resize(strLen);
		data.Read(ret[i].data(), strLen);
	}
	return ret;
}

// the object will only be added if it can be deserialized in its entirety (children must also be valid)
static void ReadFullObject(DataArchiveFile& file, const BinarySpan& data, std::vector<ObjectCreationData>& outDst)
{
	ObjectCreationData creationData{};
	if (!Object::DeserializeIntoCreationData(data, creationData))
	{
		Console::WriteLine("failed to deserialize unknown object", Console::Severity::Error);
		return;
	}

	std::string references = creationData.name + "_ref_children";
	if (!file.HasEntry(references)) // not an error, since its optional to have children
	{
		outDst.push_back(creationData);
		return;
	}

	std::expected<std::vector<char>, bool> childRefs = file.ReadData(creationData.name + "_ref_children");
	if (!childRefs.has_value())
	{
		outDst.push_back(creationData);
		Console::WriteLine("failed to read child references for {}", Console::Severity::Error, creationData.name);
		return;
	}

	BinarySpan asSpan = *childRefs;
	std::vector<std::string> children = ReadNamedReferences(asSpan);

	creationData.children.reserve(children.size());
	for (const std::string& child : children)
	{
		std::expected<std::vector<char>, bool> objectData = file.ReadData(child);
		if (objectData.has_value())
			ReadFullObject(file, *objectData, creationData.children);
		else
			Console::WriteLine("failed to read child object \"{}\"", Console::Severity::Error, child);
	}

	outDst.push_back(creationData);
}

void SceneLoader::LoadObjectsFromArchive(DataArchiveFile& file)
{
	std::expected<std::vector<char>, bool> root = file.ReadData("##object_root");
	if (!root.has_value())
	{
		Console::WriteLine("no object root found for {}", Console::Severity::Warning, location); // scenes dont need to have objects, but itd be weird not to have any
		return;
	}

	std::vector<std::string> childReferences = ReadNamedReferences(*root);
	objects.reserve(childReferences.size());

	for (const std::string& child : childReferences)
	{
		std::expected<std::vector<char>, bool> data = file.ReadData(child);
		if (data.has_value())
			ReadFullObject(file, *data, objects);
		else
			Console::WriteLine("failed to read base object \"{}\"", Console::Severity::Error, child);
	}
}

static FileImage DeserializeImage(const BinarySpan& span)
{
	FileImage ret{};
	span >> ret.width >> ret.height;

	size_t size = ret.width * ret.height * 4;

	if (size == 0)
		return ret;

	ret.data.data.resize(size);
	span.Read(ret.data.data.data(), size);

	return ret;
}

static MaterialCreationData DeserializeMaterial(const BinarySpan& data)
{
	MaterialCreationData ret{};
	ret.albedo = DeserializeImage(data);
	ret.normal = DeserializeImage(data);
	ret.metallic = DeserializeImage(data);
	ret.roughness = DeserializeImage(data);
	ret.ambientOccl = DeserializeImage(data);

	return ret;
}

void SceneLoader::LoadMaterialsFromArchive(DataArchiveFile& file)
{
	std::expected<std::vector<char>, bool> root = file.ReadData("##material_root");
	if (!root.has_value())
	{
		Console::WriteLine("no material root found for {}", Console::Severity::Warning, location);
		return;
	}

	std::vector<std::string> references = ReadNamedReferences(*root);
	materials.reserve(references.size());

	for (const std::string& materialName : references)
	{
		std::expected<std::vector<char>, bool> data = file.ReadData(materialName);
		if (data.has_value())
			materials.push_back(DeserializeMaterial(*data));
		else
			Console::WriteLine("failed to read material {}", Console::Severity::Error, materialName);
	}
}

void SceneLoader::LoadCustomFile()
{
	DataArchiveFile file(location, DataArchiveFile::OpenMethod::Append);

	LoadObjectsFromArchive(file);
	LoadMaterialsFromArchive(file);
}

static glm::vec3 ConvertAiVec3(const aiVector3D& vec)
{
	return { vec.x, vec.y, vec.z };
}

static void RetrieveVertices(aiMesh* pMesh, std::vector<Vertex>& dst, glm::vec3& min, glm::vec3& max)
{
	dst.reserve(dst.size() + pMesh->mNumVertices);
	for (unsigned int i = 0; i < pMesh->mNumVertices; i++)
	{
		Vertex vertex{};

		vertex.position = ConvertAiVec3(pMesh->mVertices[i]);

		if (pMesh->HasNormals())
			vertex.normal = ConvertAiVec3(pMesh->mNormals[i]);

		if (pMesh->mTextureCoords[0])
			vertex.textureCoordinates = ConvertAiVec3(pMesh->mTextureCoords[0][i]);

		if (pMesh->HasTangentsAndBitangents())
		{
			vertex.tangent = ConvertAiVec3(pMesh->mTangents[i]);
			vertex.biTangent = ConvertAiVec3(pMesh->mBitangents[i]);
		}

		min = glm::min(vertex.position, min);
		max = glm::max(vertex.position, max);
		
		dst.push_back(vertex);
	}
}

static std::vector<Vertex> RetrieveVertices(aiMesh* pMesh, glm::vec3& min, glm::vec3& max)
{
	std::vector<Vertex> ret;

	RetrieveVertices(pMesh, ret, min, max);

	return ret;
}

static void RetrieveIndices(aiMesh* pMesh, std::vector<uint32_t>& dst, uint32_t offset)
{
	dst.reserve(dst.size() + pMesh->mNumFaces * 3);
	for (int i = 0; i < pMesh->mNumFaces; i++)
	{
		for (int j = 0; j < pMesh->mFaces[i].mNumIndices; j++)
		{
			uint32_t index = pMesh->mFaces[i].mIndices[j];
			dst.push_back(offset + index);
		}
	}
}

static std::vector<uint32_t> RetrieveIndices(aiMesh* pMesh)
{
	std::vector<uint32_t> ret;
	RetrieveIndices(pMesh, ret, 0);
	return ret;
}

static glm::mat4 GetMat4(const aiMatrix4x4& from)
{
	glm::mat4 to{};
	//the a,b,c,d in assimp is the row ; the 1,2,3,4 is the column
	to[0][0] = from.a1; to[1][0] = from.a2; to[2][0] = from.a3; to[3][0] = from.a4;
	to[0][1] = from.b1; to[1][1] = from.b2; to[2][1] = from.b3; to[3][1] = from.b4;
	to[0][2] = from.c1; to[1][2] = from.c2; to[2][2] = from.c3; to[3][2] = from.c4;
	to[0][3] = from.d1; to[1][3] = from.d2; to[2][3] = from.d3; to[3][3] = from.d4;
	return to;
}

static aiMatrix4x4 GetMatrix4x4(const glm::mat4& from)
{
	aiMatrix4x4 to{};
	//the a,b,c,d in assimp is the row ; the 1,2,3,4 is the column
	to.a1 = from[0][0]; to.a2 = from[1][0]; to.a3 = from[2][0]; to.a4 = from[3][0];
	to.b1 = from[0][1]; to.b2 = from[1][1]; to.b3 = from[2][1]; to.b4 = from[3][1];
	to.c1 = from[0][2]; to.c2 = from[1][2]; to.c3 = from[2][2]; to.c4 = from[3][2];
	to.d1 = from[0][3]; to.d2 = from[1][3]; to.d3 = from[2][3]; to.d4 = from[3][3];
	return to;
}

static void SetVertexBones(Vertex& vertex, int ID, float weight)
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

	aiBone** bones = pMesh->mBones;

	for (int i = 0; i < pMesh->mNumBones; i++)
	{
		int ID = -1;
		std::string name = bones[i]->mName.C_Str();
		if (boneInfoMap.find(name) == boneInfoMap.end())
		{
			BoneInfo info{};
			info.index = i;
			info.offset = GetMat4(pMesh->mBones[i]->mOffsetMatrix);

			boneInfoMap[name] = info;
			ID = i;
		}
		else ID = boneInfoMap[name].index;

		if (ID == -1) 
			throw std::runtime_error("Failed to retrieve bone data");

		aiVertexWeight* weights = bones[i]->mWeights;
		
		for (int j = 0; j < bones[i]->mNumWeights; j++)
			if (weights[j].mWeight != 0.0f)
				SetVertexBones(creationData.vertices[weights[j].mVertexId], ID, weights[j].mWeight);
	}
}

MeshCreationData SceneLoader::RetrieveMeshData(aiMesh* pMesh)
{
	MeshCreationData ret{};

	ret.faceCount = pMesh->mNumFaces;
	ret.materialIndex = pMesh->mMaterialIndex + 1; // + 1 because we already have the default material loaded in
	ret.vertices = RetrieveVertices(pMesh, ret.min, ret.max);
	ret.indices = RetrieveIndices(pMesh);
	RetrieveBoneData(ret, pMesh);

	return ret;
}

void SceneLoader::MergeMeshData(MeshCreationData& dst, aiMesh* pMesh)
{
	RetrieveVertices(pMesh, dst.vertices, dst.min, dst.max);
	RetrieveIndices(pMesh, dst.indices, dst.faceCount * 3);

	RetrieveBoneData(dst, pMesh); // this is not tested enough, so it may result in weird behavior

	dst.faceCount += pMesh->mNumFaces;
}

static void GetTransform(const aiMatrix4x4& mat, glm::vec3& pos, glm::quat& rot, glm::vec3& scale)
{
	glm::mat4 trans;
	memcpy(&trans, &mat, sizeof(glm::mat4));
	trans = glm::transpose(trans);
	glm::quat orientation;
	glm::vec3 skew;
	glm::vec4 perspective;
	glm::decompose(trans, scale, orientation, pos, skew, perspective);
	rot = glm::degrees(glm::eulerAngles(orientation));
	//pos /= 100;
	//scale /= 100;
}

static std::string GetTextureFile(const aiScene* scene, aiTextureType type, int i, int index, const fs::path& base)
{
	aiString str{};
	aiReturn res = scene->mMaterials[i]->GetTexture(type, index, &str);
	return res == aiReturn_SUCCESS ? (base / str.C_Str()).string() : "";
}

void SceneLoader::LoadAssimpFile()
{
	const aiScene* scene = aiImportFile(location.c_str(), aiPostProcessSteps::aiProcess_Triangulate | aiPostProcessSteps::aiProcess_CalcTangentSpace);
	if (scene == nullptr) // check if the file could be read
		throw std::runtime_error("Failed to find or read file at " + location);

	const char* err = aiGetErrorString();
	if (err != nullptr && err[0] != '\0')
		Console::WriteLine(err, Console::Severity::Error);

	ObjectCreationData root = RetrieveObject(scene, scene->mRootNode, glm::mat4(1)); // for now we ignore the root node
	objects.push_back(root);

	if (scene->HasAnimations())
		animations.resize(scene->mNumAnimations);

	for (int i = 0; i < scene->mNumAnimations; i++)
	{
		animations.emplace_back(scene->mAnimations[i], scene->mRootNode, boneInfoMap);
	}

	fs::path baseDir = fs::path(location).parent_path();

	for (int i = 0; i < scene->mNumMaterials; i++)
	{
		MaterialCreateInfo data{};

		data.albedo = GetTextureFile(scene, aiTextureType_DIFFUSE, i, 0, baseDir);
		//data.normal = GetTextureFile(scene, aiTextureType_NORMALS, i, 0, baseDir);
		//data.roughness = GetTextureFile(scene, aiTextureType_DIFFUSE_ROUGHNESS, i, 0, baseDir);
		//data.metallic = GetTextureFile(scene, aiTextureType_METALNESS, i, 0, baseDir);
		//data.ambientOcclusion = GetTextureFile(scene, aiTextureType_AMBIENT_OCCLUSION, i, 0, baseDir);

		materials.push_back(data);
	}

	aiReleaseImport(scene);
}

static aiLight* NodeAsLight(const aiScene* scene, const aiNode* node)
{
	for (int i = 0; i < scene->mNumLights; i++)
	{
		if (node->mName == scene->mLights[i]->mName)
			return scene->mLights[i];
	}
	return nullptr;
}

ObjectCreationData SceneLoader::RetrieveObject(const aiScene* scene, const aiNode* node, glm::mat4 parentTrans)
{
	ObjectCreationData creationData;
	creationData.name = node->mName.length == 0 ? "NO_NAME" + std::to_string(unnamedObjectCount) : node->mName.C_Str();

	GetTransform(node->mTransformation, creationData.position, creationData.rotation, creationData.scale);

	aiLight* asLight = NodeAsLight(scene, node);
	if (asLight != nullptr)
	{
		//creationData.lightData.pos = glm::vec3(creationData.position);
		//creationData.lightData.type = Light::Type::Point;
		//creationData.lightData.color = glm::vec3(1); // assimp cant find the lights color !! glm::vec3(assimpLight->mColorDiffuse.r, assimpLight->mColorDiffuse.g, assimpLight->mColorDiffuse.b);
		//creationData.lightData.direction = glm::vec3(0.0f);
		//creationData.type = ObjectCreationData::Type::Light;
	}
	else
	{
		creationData.hasMesh = node->mNumMeshes > 0;

		if (creationData.hasMesh)
		{
			creationData.mesh = RetrieveMeshData(scene->mMeshes[node->mMeshes[0]]);
			creationData.type = ObjectCreationData::Type::Mesh;
		}

		for (int i = 1; i < node->mNumMeshes; i++)
		{
			ObjectCreationData child{};
			child.name = creationData.name + std::to_string(i);
			child.mesh = RetrieveMeshData(scene->mMeshes[node->mMeshes[i]]);
			child.type = ObjectCreationData::Type::Mesh;
			child.hasMesh = true;

			creationData.children.push_back(child);
		}
	}

	if (node->mNumChildren > 0)
		creationData.children.reserve(creationData.children.size() + node->mNumChildren);

	for (int i = 0; i < node->mNumChildren; i++)
		creationData.children.push_back(RetrieveObject(scene, node->mChildren[i], GetMat4(node->mTransformation)));
	
	return creationData;
}

static MeshCreationData GetMeshFromAssimp(aiMesh* pMesh)
{
	MeshCreationData ret{};

	ret.faceCount = pMesh->mNumFaces;
	ret.vertices = RetrieveVertices(pMesh, ret.min, ret.max);
	ret.indices = RetrieveIndices(pMesh);

	return ret;
}

static glm::vec3 GetExtentsFromMesh(aiMesh* pMesh)
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


ObjectCreationData assetImport::LoadObjectFile(std::string path) // kinda funky now, maybe make the funcion return multiple objects instead of one
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
	ret.type = ObjectCreationData::Type::Mesh;

	return ret;
}

glm::vec3 assetImport::LoadHitBox(std::string path)
{
	const aiScene* scene = aiImportFile(path.c_str(), aiProcessPreset_TargetRealtime_Fast);

	if (scene == nullptr)
		throw std::runtime_error("Cannot load a file: the file is invalid");
	if (scene->mNumMeshes <= 0)
		throw std::runtime_error("Cannot get the hitbox from a file: there are no meshes to get data from");
	return GetExtentsFromMesh(scene->mMeshes[0]);
}

MeshCreationData assetImport::LoadFirstMesh(const std::string& file)
{
	const aiScene* scene = aiImportFile(file.c_str(), aiProcessPreset_TargetRealtime_Fast | aiPostProcessSteps::aiProcess_OptimizeMeshes);

	if (scene == nullptr || scene->mNumMeshes <= 0)
		return {};

	return GetMeshFromAssimp(scene->mMeshes[0]);
}