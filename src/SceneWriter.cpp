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
	BinaryWriter writer(destination);
	for (Object* object : scene->allObjects)
		WriteObject(writer, object);
}

constexpr uint64_t SIZE_OF_NODE_HEADER = sizeof(NodeType) + sizeof(NodeSize);

template<typename Type>
inline NodeSize GetArrayNodeSize(const std::vector<Type> vec) { return SIZE_OF_NODE_HEADER + vec.size() * sizeof(Type); }
inline NodeSize GetNameNodeSize(const std::string& name)      { return SIZE_OF_NODE_HEADER + name.size() + 1; }
inline NodeSize GetMeshNodeSize(const Mesh& mesh)             { return SIZE_OF_NODE_HEADER + GetArrayNodeSize(mesh.vertices) + GetArrayNodeSize(mesh.indices) + sizeof(uint32_t); }
inline NodeSize GetObjectNodeSize(const Object* object)       { return SIZE_OF_NODE_HEADER + GetNameNodeSize(object->name) + GetMeshNodeSize(object->meshes[0]); } // should account for all meshes
inline NodeSize GetTransformNodeSize()                        { return SIZE_OF_NODE_HEADER + sizeof(glm::vec3) * 3; }
inline NodeSize GetRigidBodyNodeSize()                        { return SIZE_OF_NODE_HEADER + sizeof(uint8_t) * 2 + sizeof(glm::vec3); }

inline void WriteMesh(BinaryWriter& writer, const Mesh& mesh)
{
	writer 
		<< NODE_TYPE_MESH << GetMeshNodeSize(mesh) 
		<< mesh.materialIndex
		<< NODE_TYPE_VERTICES << mesh.vertices.size() * sizeof(mesh.vertices[0]) << mesh.vertices // vertices array
		<< NODE_TYPE_INDICES << mesh.indices.size() * sizeof(mesh.indices[0]) << mesh.indices;    // indices array
}

inline void WriteRigidBody(BinaryWriter& writer, const RigidBody& rigid)
{
	writer
		<< NODE_TYPE_RIGIDBODY << GetRigidBodyNodeSize()
		<< rigid.type        // rigid body type
		<< rigid.shape.type  // shape type
		<< rigid.shape.data; // shape extents
}

inline void WriteTransform(BinaryWriter& writer, const Transform& transform)
{
	writer 
		<< NODE_TYPE_TRANSFORM << GetTransformNodeSize() 
		<< transform.position 
		<< transform.rotation 
		<< transform.scale;
}

void HSFWriter::WriteObject(BinaryWriter& writer, Object* object)
{
	writer << NODE_TYPE_OBJECT << SIZE_OF_NODE_HEADER + GetObjectNodeSize(object) << object->state << NODE_TYPE_NAME << object->name.size() + 1 << object->name;

	WriteTransform(writer, object->transform);
	WriteRigidBody(writer, object->rigid);
	WriteMesh(writer, object->meshes[0]);
}