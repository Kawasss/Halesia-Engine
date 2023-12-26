#include <cstdint>
#include "io/SceneWriter.h"
#include "io/SceneLoader.h"
#include "Scene.h"
#include "Object.h"

struct HSFHeader
{
	uint32_t version = 1;
};

inline RigidCreationData GetRigidCreationData(RigidBody& rigid)
{
	return RigidCreationData{ rigid.shape.data, rigid.shape.type, rigid.type };
}

inline MeshCreationData GetMeshCreationData(Mesh& mesh)
{
	MeshCreationData creationData{};
	creationData.amountOfVertices = mesh.vertices.size();
	creationData.vertices = mesh.vertices;
	creationData.faceCount = mesh.faceCount;
	creationData.indices = mesh.indices;
	creationData.center = mesh.center;
	creationData.extents = mesh.extents;
	creationData.hasBones = 0;
	creationData.hasMaterial = 0;
	creationData.name = mesh.name;

	return creationData;
}

void HSFWriter::WriteHSFScene(Scene* scene, std::string destination)
{

}