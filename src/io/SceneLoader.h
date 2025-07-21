#pragma once
#include <string>
#include <vector>
#include <map>
#include <set>
#include <variant>

#include "../glm.h"

#include "../renderer/Bone.h"
#include "../renderer/AnimationManager.h"
#include "../renderer/Light.h"
#include "../renderer/Material.h"

#include "FileFormat.h"
#include "CreationData.h"

constexpr int textureCoordinateOffset = 12;
constexpr int normalOffset = 20;

struct aiMesh;
struct aiNode;
struct aiScene;

class DataArchiveFile;

class SceneLoader
{
public:
	SceneLoader() = default;
	SceneLoader(std::string sceneLocation);

	void LoadScene();

	std::vector<ObjectCreationData> objects;
	std::vector<std::variant<MaterialCreationData, MaterialCreateInfo>> materials;

	// animations
	std::vector<Animation> animations;
	std::map<std::string, BoneInfo> boneInfoMap;

private:
	// file specific info
	std::string header;
	std::string location;

	void LoadCustomFile();
	void LoadAssimpFile();

	void LoadObjectsFromArchive(DataArchiveFile& file);
	void LoadMaterialsFromArchive(DataArchiveFile& file);

	void RetrieveBoneData(MeshCreationData& creationData, const aiMesh* pMesh);
	MeshCreationData RetrieveMeshData(aiMesh* pMesh);
	void MergeMeshData(MeshCreationData& dst, aiMesh* pMesh);

	void GetNodeHeader(NodeType& type, NodeSize& size);
	void RetrieveType(NodeType type, NodeSize size);
	ObjectCreationData RetrieveObject(const aiScene* scene, const aiNode* node, glm::mat4 parentTrans); // can return multiple objects if this one node has multiple meshes, but must of the time its one object

	std::vector<ObjectCreationData>::iterator currentObject{};

	int unnamedObjectCount = 0; // used for creating uniques names for unnamed objects
};

namespace assetImport
{
	glm::vec3 LoadHitBox(std::string path);
	ObjectCreationData LoadObjectFile(std::string path);

	MeshCreationData LoadFirstMesh(const std::string& file);
}