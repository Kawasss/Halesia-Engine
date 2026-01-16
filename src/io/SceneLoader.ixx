module;

#include "../glm.h"

#include "../renderer/Light.h"
#include "../renderer/Material.h"

#include "FileFormat.h"
#include "CreationData.h"
#include "BinaryStream.h"

export module IO.SceneLoader;

import <assimp/scene.h>;

import std;

import IO.DataArchiveFile;

import Renderer.Animation;
import Renderer.Bone;

export class SceneLoader
{
public:
	SceneLoader() = default;
	SceneLoader(std::string sceneLocation);

	void LoadScene();

	std::vector<ObjectCreationData> objects;
	std::vector<std::variant<MaterialCreationData, MaterialCreateInfo>> materials;

	// animations
	std::vector<Animation> animations;

	size_t objectCount = 0; // the amount of objects read (not the same as objects.size(), which contains children)

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

	void ReadFullObject(DataArchiveFile& file, const BinarySpan& data, std::vector<ObjectCreationData>& outDst);

	std::vector<ObjectCreationData>::iterator currentObject{};

	int unnamedObjectCount = 0; // used for creating uniques names for unnamed objects
};

export namespace assetImport
{
	glm::vec3 LoadHitBox(std::string path);
	ObjectCreationData LoadObjectFile(std::string path);

	MeshCreationData LoadFirstMesh(const std::string& file);
}