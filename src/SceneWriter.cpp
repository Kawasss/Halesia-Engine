#include <cstdint>
#include <sstream>
#include <Windows.h>
#include <compressapi.h>

#include "io/CreationData.h"
#include "io/SceneWriter.h"
#include "io/BinaryWriter.h"

#include "core/Scene.h"
#include "core/Object.h"

#include "physics/RigidBody.h"

#include "renderer/Mesh.h"

typedef unsigned char byte;

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

constexpr uint64_t SIZE_OF_NODE_HEADER = sizeof(NodeType) + sizeof(NodeSize);

template<typename Type>
inline NodeSize GetArrayNodeSize(const std::vector<Type> vec) { return vec.size() * sizeof(Type); }
inline NodeSize GetNameNodeSize(const std::string& name)      { return name.size() + 1; }
inline NodeSize GetMeshNodeSize(const Mesh& mesh)             { return GetArrayNodeSize(mesh.vertices) + GetArrayNodeSize(mesh.indices) + sizeof(uint32_t); }
inline NodeSize GetObjectNodeSize(const Object* object)       { return GetNameNodeSize(object->name) + (object->mesh.IsValid() ? GetMeshNodeSize(object->mesh) : 0); } // should account for all meshes
inline NodeSize GetTransformNodeSize()                        { return sizeof(glm::vec3) * 3; }
inline NodeSize GetRigidBodyNodeSize()                        { return sizeof(uint8_t) * 2 + sizeof(glm::vec3); }

inline void WriteTexture(BinaryWriter& writer, const std::vector<uint8_t>& compressedData, uint32_t width, uint32_t height, NodeType type)
{
	writer << type << compressedData.size() + sizeof(uint32_t) * 2 << width << height << compressedData;
}

inline void WriteMaterial(BinaryWriter& writer, const Material& material)
{
	std::vector<uint8_t> albedo = material.albedo->GetImageData(), normal = material.normal->GetImageData(), roughness = material.roughness->GetImageData();
	std::vector<uint8_t> metallic = material.metallic->GetImageData(), ao = material.ambientOcclusion->GetImageData();

	writer << NODE_TYPE_MATERIAL << SIZE_OF_NODE_HEADER * 5 + albedo.size() + normal.size() + roughness.size() + metallic.size() + ao.size() + sizeof(material.isLight) << material.isLight;
	WriteTexture(writer, albedo, material.albedo->GetWidth(), material.albedo->GetHeight(), NODE_TYPE_ALBEDO);
	WriteTexture(writer, normal, material.normal->GetWidth(), material.normal->GetHeight(), NODE_TYPE_NORMAL);
	WriteTexture(writer, roughness, material.roughness->GetWidth(), material.roughness->GetHeight(), NODE_TYPE_ROUGHNESS);
	WriteTexture(writer, metallic, material.metallic->GetWidth(), material.metallic->GetHeight(), NODE_TYPE_METALLIC);
	WriteTexture(writer, ao, material.ambientOcclusion->GetWidth(), material.ambientOcclusion->GetHeight(), NODE_TYPE_AMBIENT_OCCLUSION);
}

void HSFWriter::WriteHSFScene(Scene* scene, std::string destination)
{
	BinaryWriter writer(destination);
	for (Object* object : scene->allObjects)
		WriteObject(writer, object);
	for (int i = 1; i < Mesh::materials.size(); i++)
		WriteMaterial(writer, Mesh::materials[i]);
	writer.WriteDataToFile(NODE_TYPE_COMPRESSION).WriteDataToFile(sizeof(uint64_t) + sizeof(uint32_t)).WriteDataToFile(writer.GetCurrentSize()).WriteDataToFile(COMPRESS_ALGORITHM_XPRESS);
	writer.WriteToFileCompressed();
}

inline void WriteMesh(BinaryWriter& writer, Mesh& mesh)
{
	writer 
		<< NODE_TYPE_MESH << GetMeshNodeSize(mesh) 
		<< mesh.GetMaterialIndex()
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

	if (!object->mesh.IsValid())
		return;

	WriteMesh(writer, object->mesh);
}