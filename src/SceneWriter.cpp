#include <cstdint>
#include <iostream>

#include "io/SceneWriter.h"
#include "io/SceneLoader.h"

#include "core/Scene.h"
#include "core/Object.h"

#include "physics/RigidBody.h"

#include "renderer/Mesh.h"

typedef unsigned char byte;

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

void SerializeName(std::ofstream& stream, std::string name)
{
	NodeType type = NODE_TYPE_NAME;
	NodeSize size = name.size() + 1;
	stream.write((char*)&type, sizeof(type));
	stream.write((char*)&size, sizeof(size));
	stream.write(name.c_str(), name.size() + 1); // string has to be written seperately
}

void HSFWriter::WriteObject(Object* object, std::string destination)
{
	std::ofstream stream(destination, std::ios::binary);
	
	NodeType type = NODE_TYPE_OBJECT;
	stream.write((char*)&type, sizeof(type));
	SerializeName(stream, object->name);
}