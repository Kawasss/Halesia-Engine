#pragma comment(lib, "cabinet.lib") // needed for compression
#include <cstdint>
#include <iostream>
#include <sstream>
#include <Windows.h>
#include <compressapi.h>

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
	BinaryWriter(std::string destination) : output(std::ofstream(destination, std::ios::binary)) {}

	void Write(const char* ptr, size_t size)
	{
		stream.write(ptr, size);
	}

	template<typename Type>
	BinaryWriter& WriteDataToFile(const Type& data)
	{
		output.write(reinterpret_cast<const char*>(&data), sizeof(Type));
		return *this;
	}

	template<typename Type>
	BinaryWriter& operator<<(const Type& value)
	{
		stream.write(reinterpret_cast<const char*>(&value), sizeof(Type));
		return *this;
	}

	template<typename Type>
	BinaryWriter& operator<<(const std::vector<Type>& vector)
	{
		if (vector.empty()) return *this;
		stream.write((const char*)&vector[0], sizeof(Type) * vector.size());
		return *this;
	}

	BinaryWriter& operator<<(const std::string& str)
	{
		stream.write(str.c_str(), str.size() + 1); // also writes the null character
		return *this;
	}

	void WriteToFileCompressed()
	{
		size_t size = stream.seekg(0, std::ios::end).tellg();
		stream.seekg(0, std::ios::beg);
		char* data = new char[size];
		stream.read(data, size); // copying the stream into a usable buffer

		size_t compressedSize = 0;
		COMPRESSOR_HANDLE compressor; 
		char* compressed = new char[size]; // size is the max size, not the actual compressed size
		if (!CreateCompressor(COMPRESS_ALGORITHM_XPRESS, NULL, &compressor)) throw std::runtime_error("Cannot create a compressor"); // XPRESS is fast but not the best compression, XPRESS with huffman has better compression but is slower, MSZIP uses more resources and LZMS is slow. its Using xpress right now since its the fastest
		if (!Compress(compressor, data, size, compressed, size, &compressedSize)) throw std::runtime_error("Cannot compress");
		if (!CloseCompressor(compressor)) throw std::runtime_error("Cannot close a compressor"); // currently not checking the return value

		output.write(compressed, compressedSize);
		output.close();
		delete[] data;
		delete[] compressed;
	}

	void WriteToFileUncompressed()
	{
		size_t size = stream.seekg(0, std::ios::end).tellg();
		stream.seekg(0, std::ios::beg);
		char* data = new char[size];
		stream.read(data, size); // copying the stream into a usable buffer

		output.write(data, size);
		output.close();
		delete[] data;
	}

	size_t GetCurrentSize() { size_t ret = stream.seekg(0, std::ios::end).tellg(); stream.seekg(0); return ret; }

private:
	std::stringstream stream;
	std::ofstream output;
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

constexpr uint64_t SIZE_OF_NODE_HEADER = sizeof(NodeType) + sizeof(NodeSize);

template<typename Type>
inline NodeSize GetArrayNodeSize(const std::vector<Type> vec) { return vec.size() * sizeof(Type); }
inline NodeSize GetNameNodeSize(const std::string& name)      { return name.size() + 1; }
inline NodeSize GetMeshNodeSize(const Mesh& mesh)             { return GetArrayNodeSize(mesh.vertices) + GetArrayNodeSize(mesh.indices) + sizeof(uint32_t); }
inline NodeSize GetObjectNodeSize(const Object* object)       { return GetNameNodeSize(object->name) + GetMeshNodeSize(object->meshes[0]); } // should account for all meshes
inline NodeSize GetTransformNodeSize()                        { return sizeof(glm::vec3) * 3; }
inline NodeSize GetRigidBodyNodeSize()                        { return sizeof(uint8_t) * 2 + sizeof(glm::vec3); }

inline void WriteTexture(BinaryWriter& writer, const std::vector<uint8_t>& compressedData, NodeType type)
{
	writer << type << compressedData.size() << compressedData;
}

inline void WriteMaterial(BinaryWriter& writer, const Material& material)
{
	std::vector<uint8_t> albedo = material.albedo->GetImageDataAsPNG(), normal = material.normal->GetImageDataAsPNG(), roughness = material.roughness->GetImageDataAsPNG();
	std::vector<uint8_t> metallic = material.metallic->GetImageDataAsPNG(), ao = material.ambientOcclusion->GetImageDataAsPNG();

	writer << NODE_TYPE_MATERIAL << SIZE_OF_NODE_HEADER * 5 + albedo.size() + normal.size() + roughness.size() + metallic.size() + ao.size() + sizeof(material.isLight) << material.isLight;
	WriteTexture(writer, albedo, NODE_TYPE_ALBEDO);
	WriteTexture(writer, normal, NODE_TYPE_NORMAL);
	WriteTexture(writer, roughness, NODE_TYPE_ROUGHNESS);
	WriteTexture(writer, metallic, NODE_TYPE_METALLIC);
	WriteTexture(writer, ao, NODE_TYPE_AMBIENT_OCCLUSION);
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