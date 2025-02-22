#pragma once
#include <string>
#include <vector>
#include <map>

#include "../glm.h"

#include "../renderer/Bone.h"
#include "../renderer/AnimationManager.h"
#include "../renderer/Light.h"

#include "BinaryReader.h"
#include "FileFormat.h"
#include "CreationData.h"

constexpr int textureCoordinateOffset = 12;
constexpr int normalOffset = 20;

struct aiMesh;
struct aiNode;
struct aiScene;

class SceneLoader
{
public:
	SceneLoader() = default;
	SceneLoader(std::string sceneLocation);

	void LoadScene();

	std::vector<ObjectCreationData> objects;
	std::vector<MaterialCreationData> materials;

	std::vector<Light> lights;

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

	void GetNodeHeader(NodeType& type, NodeSize& size);
	void RetrieveType(NodeType type, NodeSize size);
	ObjectCreationData RetrieveObject(const aiScene* scene, const aiNode* node, glm::mat4 parentTrans);

	std::vector<ObjectCreationData>::iterator currentObject{};
};

namespace GenericLoader
{
	glm::vec3 LoadHitBox(std::string path);
	ObjectCreationData LoadObjectFile(std::string path);
}