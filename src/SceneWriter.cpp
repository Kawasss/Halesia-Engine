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

class BinaryWriter
{
public:
	BinaryWriter(std::string destination) : stream(std::ofstream(destination, std::ios::binary)) {}

	template<typename Type>
	BinaryWriter& operator<<(Type value)
	{
		stream.write(reinterpret_cast<char*>(&value), sizeof(Type));
		return *this;
	}

	template<typename Type>
	BinaryWriter& operator<<(const std::vector<Type>& vector)
	{
		stream.write((char*)&vector[0], sizeof(Type)* vector.size());
		return *this;
	}

	BinaryWriter& operator<<(std::string str)
	{
		stream.write(str.c_str(), str.size() + 1); // also writes the null character
		return *this;
	}

private:
	std::ofstream stream;
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

constexpr uint64_t SIZE_OF_NODE_HEADER = sizeof(NodeType) + sizeof(NodeSize);

template<typename Type>
inline NodeSize GetArrayNodeSize(const std::vector<Type> vec)
{
	return SIZE_OF_NODE_HEADER + vec.size() * sizeof(Type);
}

inline NodeSize GetNameNodeSize(const std::string& name)
{
	return SIZE_OF_NODE_HEADER + name.size() + 1;
}

inline NodeSize GetMeshNodeSize(const Mesh& mesh)
{
	return SIZE_OF_NODE_HEADER + GetArrayNodeSize(mesh.vertices) + GetArrayNodeSize(mesh.indices);
}

inline NodeSize GetObjectNodeSize(const Object* object)
{
	return SIZE_OF_NODE_HEADER + GetNameNodeSize(object->name) + GetMeshNodeSize(object->meshes[0]); // should account for all meshes
}

void HSFWriter::WriteObject(Object* object, std::string destination)
{
	BinaryWriter writer(destination);

	writer << NODE_TYPE_OBJECT << SIZE_OF_NODE_HEADER + GetObjectNodeSize(object) << NODE_TYPE_NAME << object->name.size() + 1 << object->name;

	// mesh testing
	Mesh& mesh = object->meshes[0];

	writer << NODE_TYPE_MESH << GetMeshNodeSize(mesh);

	// vertices
	writer << NODE_TYPE_ARRAY << mesh.vertices.size() << mesh.vertices;
	//indices
	writer << NODE_TYPE_ARRAY << mesh.indices.size() << mesh.indices;
}